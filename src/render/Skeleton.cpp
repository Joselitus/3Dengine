#include "Skeleton.h"

#include <cmath>

Skeleton::Skeleton()
    : globalInverseTransform(1.0f), boneMats(MAX_BONES, glm::mat4(1.0f)),
      root(nullptr), animation(nullptr) {}

void Skeleton::Init(aiNode *in_root, const aiAnimation *in_animation,
                    std::vector<BoneInfo> in_bones) {
  root = in_root;
  animation = in_animation;
  bones = in_bones;
  // The root transform is part of every node global, so it is kept in the
  // result (no globalInverse cancelling it).
  globalInverseTransform = glm::mat4(1.0f);

  // The bind pose of each bone (the inverse of its offset) and its nearest ancestor that is a bone
  bindGlobals.clear();
  boneParents.clear();
  for (const BoneInfo &bone : bones)
    bindGlobals[bone.name] = glm::inverse(globalInverseTransform) * glm::inverse(bone.offset);
  for (const BoneInfo &bone : bones)
    for (const aiNode *up = bone.node ? bone.node->mParent : nullptr; up; up = up->mParent)
      if (bindGlobals.count(up->mName.data)) {
        boneParents[bone.name] = up->mName.data;
        break;
      }

  channels.clear();
  if (animation)
    for (unsigned int i = 0; i < animation->mNumChannels; i++)
      channels[animation->mChannels[i]->mNodeName.data] =
          animation->mChannels[i];

  Update(0.0);
}

// The file's own matrix of a node in its static pose (the root's included)
static glm::mat4 StaticGlobal(const aiNode *node) {
  glm::mat4 m(1.0f);
  for (; node; node = node->mParent) {
    aiMatrix4x4 local = node->mTransformation;
    m = AiToGLMMat4(local) * m;
  }
  return m;
}

void Skeleton::SetBindPoses(const std::unordered_map<std::string, glm::mat4> &offsets) {
  // What takes the mesh's own space (where the offsets are) to the skin's (where a bone's matrix is,
  // with the units and axes the file's root node gives): a weighted bone's static matrix times its
  // offset. Bones are posed in the skin's space, so their bind matrices are too.
  glm::mat4 toSkin(1.0f);
  for (const BoneInfo &bone : bones)
    if (bone.node) {
      toSkin = StaticGlobal(bone.node) * bone.offset;
      break;
    }
  for (const auto &entry : offsets)
    bindGlobals[entry.first] = glm::inverse(globalInverseTransform) * toSkin * glm::inverse(entry.second);
  boneParents.clear();
  for (const auto &entry : bindGlobals) {
    const aiNode *node = root ? root->FindNode(entry.first.c_str()) : nullptr;
    for (const aiNode *up = node ? node->mParent : nullptr; up; up = up->mParent)
      if (bindGlobals.count(up->mName.data)) {
        boneParents[entry.first] = up->mName.data;
        break;
      }
  }
}

bool Skeleton::PoseGlobal(const std::string &name, glm::mat4 &matrix) const {
  std::string up = name;
  for (int guard = 0; guard < 256; guard++) {
    for (size_t i = 0; i < bones.size() && i < MAX_BONES; i++)
      if (bones[i].name == up) {
        // skin = inverse-root * M * offset, so M is the matrix of a weighted bone
        glm::mat4 skinned = glm::inverse(globalInverseTransform) * boneMats[i] * glm::inverse(skinFix) * glm::inverse(bones[i].offset);
        if (up == name) {
          matrix = skinned;
          return true;
        }
        glm::mat4 bindUp, bindMe;
        if (!BindGlobal(up, bindUp) || !BindGlobal(name, bindMe))
          return false;
        matrix = skinned * glm::inverse(bindUp) * bindMe;
        return true;
      }
    auto parent = boneParents.find(up);
    if (parent == boneParents.end())
      return false;
    up = parent->second;
  }
  return false;
}

bool Skeleton::BindGlobal(const std::string &name, glm::mat4 &matrix) const {
  auto it = bindGlobals.find(name);
  if (it == bindGlobals.end())
    return false;
  matrix = it->second;
  return true;
}

void Skeleton::SetPose(const std::unordered_map<std::string, glm::mat4> &globals,
                       const std::string &orphansFollow) {
  // the named bones as they are told, the rest following their nearest named ancestor
  std::unordered_map<std::string, glm::mat4> resolved = globals;
  std::vector<std::string> chain;
  for (const BoneInfo &bone : bones) {
    if (resolved.count(bone.name))
      continue;
    chain.clear();
    std::string name = bone.name;
    while (!resolved.count(name)) {
      chain.push_back(name);
      auto up = boneParents.find(name);
      if (up == boneParents.end())
        break;
      name = up->second;
    }
    if (!resolved.count(name)) {
      // no ancestor was posed: it follows `orphansFollow` if it is told to, or keeps its last pose
      auto bind = bindGlobals.find(bone.name);
      auto lead = resolved.find(orphansFollow);
      auto leadBind = bindGlobals.find(orphansFollow);
      if (orphansFollow.empty() || lead == resolved.end() || leadBind == bindGlobals.end() || bind == bindGlobals.end())
        continue;
      resolved[bone.name] = lead->second * glm::inverse(leadBind->second) * bind->second;
      // (and the ones that hung from it, in the chain, were not resolved: they will be, as they come)
      continue;
    }
    // from the posed ancestor down: M = M_up * bind_up^-1 * bind
    for (size_t i = chain.size(); i-- > 0;) {
      const std::string &child = chain[i];
      resolved[child] = resolved[name] * glm::inverse(bindGlobals[name]) * bindGlobals[child];
      name = child;
    }
  }
  for (size_t i = 0; i < bones.size() && i < MAX_BONES; i++) {
    auto it = resolved.find(bones[i].name);
    if (it != resolved.end())
      boneMats[i] = globalInverseTransform * it->second * bones[i].offset * skinFix;
  }
}

glm::mat4 Skeleton::NodeGlobal(const aiNode *node) const {
  auto it = nodeGlobals.find(node);
  return it == nodeGlobals.end() ? glm::mat4(1.0f) : it->second;
}

// Index of the last key whose time is <= ticks (clamped so index+1 is valid).
template <typename Key>
static unsigned int FindKey(const Key *keys, unsigned int count, double ticks) {
  for (unsigned int i = 0; i + 1 < count; i++)
    if (ticks < keys[i + 1].mTime)
      return i;
  return count - 2;
}

static glm::vec3 SampleVec(const aiVectorKey *keys, unsigned int count,
                           double ticks) {
  if (count == 1)
    return glm::vec3(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
  unsigned int i = FindKey(keys, count, ticks);
  double dt = keys[i + 1].mTime - keys[i].mTime;
  float f = dt > 0 ? (float)((ticks - keys[i].mTime) / dt) : 0.0f;
  f = glm::clamp(f, 0.0f, 1.0f);
  glm::vec3 a(keys[i].mValue.x, keys[i].mValue.y, keys[i].mValue.z);
  glm::vec3 b(keys[i + 1].mValue.x, keys[i + 1].mValue.y,
              keys[i + 1].mValue.z);
  return glm::mix(a, b, f);
}

static glm::quat SampleQuat(const aiQuatKey *keys, unsigned int count,
                            double ticks) {
  auto toGlm = [](const aiQuaternion &q) {
    return glm::quat(q.w, q.x, q.y, q.z);
  };
  if (count == 1)
    return toGlm(keys[0].mValue);
  unsigned int i = FindKey(keys, count, ticks);
  double dt = keys[i + 1].mTime - keys[i].mTime;
  float f = dt > 0 ? (float)((ticks - keys[i].mTime) / dt) : 0.0f;
  f = glm::clamp(f, 0.0f, 1.0f);
  return glm::normalize(
      glm::slerp(toGlm(keys[i].mValue), toGlm(keys[i + 1].mValue), f));
}

glm::mat4 Skeleton::LocalTransform(const aiNode *node, double ticks) const {
  auto it = channels.find(node->mName.data);
  if (it == channels.end()) {
    aiMatrix4x4 t = node->mTransformation;
    return AiToGLMMat4(t);
  }
  const aiNodeAnim *ch = it->second;
  glm::vec3 pos(0.0f), scale(1.0f);
  glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
  if (ch->mNumPositionKeys)
    pos = SampleVec(ch->mPositionKeys, ch->mNumPositionKeys, ticks);
  if (ch->mNumRotationKeys)
    rot = SampleQuat(ch->mRotationKeys, ch->mNumRotationKeys, ticks);
  if (ch->mNumScalingKeys)
    scale = SampleVec(ch->mScalingKeys, ch->mNumScalingKeys, ticks);
  return glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(rot) *
         glm::scale(glm::mat4(1.0f), scale);
}

void Skeleton::Traverse(const aiNode *node, const glm::mat4 &parent,
                        double ticks) {
  glm::mat4 global = parent * LocalTransform(node, ticks);
  nodeGlobals[node] = global;
  for (unsigned int i = 0; i < node->mNumChildren; i++)
    Traverse(node->mChildren[i], global, ticks);
}

void Skeleton::Update(double seconds) {
  if (!root)
    return;

  double ticks = 0.0;
  if (animation && animation->mDuration > 0) {
    double tps = animation->mTicksPerSecond > 0 ? animation->mTicksPerSecond
                                                 : 25.0;
    ticks = std::fmod(seconds * tps, animation->mDuration);
  }

  Traverse(root, glm::mat4(1.0f), ticks);

  for (size_t i = 0; i < bones.size() && i < MAX_BONES; i++)
    boneMats[i] = globalInverseTransform * nodeGlobals[bones[i].node] *
                  bones[i].offset * skinFix;
}
