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

  channels.clear();
  if (animation)
    for (unsigned int i = 0; i < animation->mNumChannels; i++)
      channels[animation->mChannels[i]->mNodeName.data] =
          animation->mChannels[i];

  Update(0.0);
}

void Skeleton::SetPose(const std::unordered_map<std::string, glm::mat4> &globals) {
  for (size_t i = 0; i < bones.size() && i < MAX_BONES; i++) {
    auto it = globals.find(bones[i].name);
    if (it != globals.end())
      boneMats[i] = globalInverseTransform * it->second * bones[i].offset;
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
                  bones[i].offset;
}
