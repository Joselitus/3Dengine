#include "AnimatedModel.h"
#include "Gfx.h"
using namespace std;
using namespace glm;

AnimatedModel::AnimatedModel(const char *path, bool feetAtOrigin,
                             unsigned int animationIndex)
    : scene(nullptr), fitCenter(0.0f), fitScale(1.0f),
      feetAtOrigin(feetAtOrigin), animationIndex(animationIndex) {
  loadModel(path);
}

void AnimatedModel::loadModel(string path) {
  scene = importer.ReadFile(path, aiProcess_GenNormals | aiProcess_Triangulate |
                                      aiProcess_FlipUVs);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    cout << "ERROR::ASSIMP::" << importer.GetErrorString() << endl;
    scene = nullptr;
    return;
  }
  directory = path.substr(0, path.find_last_of('/'));

  // Builds meshes, and (through processAnimatedMesh) the global bone list.
  this->processNode(scene->mRootNode, scene);

  const aiAnimation *animation = chosenAnimation();
  skeleton.Init(scene->mRootNode, animation, std::move(pendingBones));
  skeleton.SetBindPoses(allOffsets);
  correctSkinScale();
  pendingBones.clear();

  computeFit();
}

// The file's static matrix of a node (the root's included)
static glm::mat4 staticGlobal(const aiNode *node) {
  glm::mat4 m(1.0f);
  for (; node; node = node->mParent) {
    aiMatrix4x4 local = node->mTransformation;
    m = AiToGLMMat4(local) * m;
  }
  return m;
}

// In a consistent file a bone's static matrix times its offset is the matrix that takes the mesh to
// the skin's space, the mesh's own node (whose transform carries the file's units and axes). With
// Assimp 5.4 and the penguin's FBX it is not: the offsets are in centimetres and the vertices are
// not, so skinned it comes out a hundred times too small and torn. Then the mesh's node is applied
// to the vertices before skinning (Skeleton::SetSkinCorrection); the idle pose already does that.
void AnimatedModel::correctSkinScale() {
  if (skeleton.bones.empty() || meshNodes.empty() || !skeleton.bones[0].node)
    return;
  glm::mat4 own = staticGlobal(skeleton.bones[0].node) * skeleton.bones[0].offset;
  glm::mat4 mesh = staticGlobal(meshNodes[0]);
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++)
      if (std::fabs(own[c][r] - mesh[c][r]) > 1e-3f * (1.0f + std::fabs(mesh[c][r]))) {
        skeleton.SetSkinCorrection(mesh);
        skeleton.Update(0.0);
        return;
      }
}

void AnimatedModel::processNode(aiNode *node, const aiScene *scene) {
  // process all the node's meshes (if any)
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    // Leftover geometry with neither bones nor a texture (e.g. an untextured
    // prop kept in the file by the exporter) is not part of the character.
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    if (mesh->mNumBones == 0 &&
        material->GetTextureCount(aiTextureType_DIFFUSE) == 0) {
      cout << "Skipping unskinned, untextured mesh " << mesh->mName.C_Str()
           << endl;
      continue;
    }
    meshes.push_back(processAnimatedMesh(mesh, scene));
    meshNodes.push_back(node);
  }
  // then do the same for each of its children
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene);
  }
}

AnimatedMesh AnimatedModel::processAnimatedMesh(aiMesh *mesh,
                                                const aiScene *scene) {
  vector<AnimatedVertex> vertices;
  vector<unsigned int> indices;
  vector<Texture> textures;

  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    AnimatedVertex vertex;
    // vertex positions, normals and texture coordinates
    vec3 vector =
        vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
    vertex.Position = vector;
    vector =
        vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
    vertex.Normal = vector;
    if (mesh->mTextureCoords[0]) { // does the mesh contain texture coordinates?
      vec2 vec;
      vec.x = mesh->mTextureCoords[0][i].x;
      vec.y = mesh->mTextureCoords[0][i].y;
      vertex.TexCoords = vec;
    } else
      vertex.TexCoords = glm::vec2(0.0f, 0.0f);
    vertices.push_back(vertex);
  }

  // Register this mesh's bones in the model-wide list; the global index is
  // what the shader uses to look the bone up in gBones.
  for (unsigned int i = 0; i < mesh->mNumBones; i++) {
    aiBone *aiBone = mesh->mBones[i];
    allOffsets[aiBone->mName.data] = AiToGLMMat4(aiBone->mOffsetMatrix);
    if (aiBone->mNumWeights == 0) // unused bone, don't spend a gBones slot
      continue;
    unsigned int globalId = pendingBones.size();

    aiMatrix4x4 offset = aiBone->mOffsetMatrix;
    BoneInfo info;
    info.name = aiBone->mName.data;
    info.node = scene->mRootNode->FindNode(aiBone->mName);
    info.offset = AiToGLMMat4(offset);
    if (pendingBones.size() >= MAX_BONES) {
      cout << "Too many bones (max " << MAX_BONES << ")" << endl;
      continue;
    }
    if (!info.node) {
      cout << "No node found for bone " << info.name << endl;
      continue;
    }
    pendingBones.push_back(info);

    for (unsigned int j = 0; j < aiBone->mNumWeights; j++) {
      aiVertexWeight weight = aiBone->mWeights[j];
      AnimatedVertex &v = vertices.at(weight.mVertexId);
      for (int k = 0; k < NUM_BONES_PER_VEREX; k++) {
        if (v.Weights[k] == 0.0f) {
          v.BoneIDs[k] = globalId;
          v.Weights[k] = weight.mWeight;
          break;
        }
      }
    }
  }

  // The exporter leaves the weights summing to anything from ~0.85 to ~1.3;
  // unnormalised weights scale the vertex towards/away from the origin of the
  // skinning transform and make it poke out of the mesh.
  for (AnimatedVertex &v : vertices) {
    float total = 0.0f;
    for (int k = 0; k < NUM_BONES_PER_VEREX; k++)
      total += v.Weights[k];
    if (total > 0.0f)
      for (int k = 0; k < NUM_BONES_PER_VEREX; k++)
        v.Weights[k] /= total;
  }

  // process indices
  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }
  // process material
  if (mesh->mMaterialIndex < scene->mNumMaterials) {
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    vector<Texture> diffuseMaps = loadMaterialTextures(
        material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    vector<Texture> specularMaps = loadMaterialTextures(
        material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
  }
  AnimatedMesh result(vertices, indices, textures);
  // A material with no texture is a flat colour (and may glow)
  if (textures.empty() && mesh->mMaterialIndex < scene->mNumMaterials) {
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    aiColor3D diffuse(1.0f, 1.0f, 1.0f), glow(0.0f, 0.0f, 0.0f);
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
      result.setColor(vec3(diffuse.r, diffuse.g, diffuse.b));
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, glow) == AI_SUCCESS &&
        (glow.r > 0.0f || glow.g > 0.0f || glow.b > 0.0f))
      result.setEmissive(vec3(glow.r, glow.g, glow.b));
  }
  return result;
}

vector<Texture> AnimatedModel::loadMaterialTextures(aiMaterial *mat,
                                                    aiTextureType type,
                                                    string typeName) {
  vector<Texture> textures;
  for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    bool skip = false;
    for (unsigned int j = 0; j < textures_loaded.size(); j++) {
      if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
        textures.push_back(textures_loaded[j]);
        skip = true;
        break;
      }
    }
    if (!skip) { // if texture hasn't been loaded already, load it
      Texture texture;
      texture.id = TextureFromFile(str.C_Str(), directory);
      texture.type = typeName;
      texture.path = str.C_Str();
      textures.push_back(texture);
      textures_loaded.push_back(texture); // add to loaded textures
    }
  }
  return textures;
}

void AnimatedModel::Update(double seconds) {
  if (!idle && !externalPose)
    skeleton.Update(seconds);
}

void AnimatedModel::setBoneGlobals(const std::map<std::string, glm::mat4> &globals,
                                   const std::string &orphansFollow) {
  externalPose = true;
  std::unordered_map<std::string, glm::mat4> byName(globals.begin(), globals.end());
  skeleton.SetPose(byName, orphansFollow);
}

bool AnimatedModel::getBoneGlobal(const std::string &name, glm::mat4 &matrix) const {
  const aiNode *node = scene ? scene->mRootNode->FindNode(name.c_str()) : nullptr;
  if (!node)
    return false;
  matrix = skeleton.NodeGlobal(node);
  return true;
}

void AnimatedModel::setIdle(bool on) {
  if (on == idle)
    return;
  idle = on;
  if (!on) {
    skeleton.Update(0.0); // the animation again (bones from its first frame)
    return;
  }
  // In the bind pose each bone's skin matrix is the same: the mesh's node
  // transform (a bone's own pose cancels with its offset matrix)
  skeleton.Update(0.0);
  idleMat = skeleton.globalInverseTransform * skeleton.NodeGlobal(meshNodes[0]);
  for (glm::mat4 &m : skeleton.boneMats)
    m = idleMat;
  // Bounds of the bind pose (the flippers' sway or hang don't change where it
  // stands): centre it and, if asked, put the feet at the origin
  glm::vec3 lo(1e30f), hi(-1e30f);
  for (const AnimatedMesh &mesh : meshes)
    for (const AnimatedVertex &v : mesh.getVertices()) {
      glm::vec3 p = glm::vec3(idleMat * glm::vec4(v.Position, 1.0f));
      lo = glm::min(lo, p);
      hi = glm::max(hi, p);
    }
  fitCenter = (lo + hi) * 0.5f;
  if (feetAtOrigin)
    fitCenter.y = lo.y;
}

const aiAnimation *AnimatedModel::chosenAnimation() const {
  if (scene->mNumAnimations == 0)
    return nullptr;
  return scene->mAnimations[std::min(animationIndex, scene->mNumAnimations - 1)];
}

void AnimatedModel::computeFit() {
  // Skin the model on the CPU at a few points of the animation just to find
  // the region it moves through.
  const aiAnimation *anim = chosenAnimation();
  double length = 0.0;
  if (anim && anim->mDuration > 0)
    length = anim->mDuration /
             (anim->mTicksPerSecond > 0 ? anim->mTicksPerSecond : 25.0);
  const int samples = length > 0.0 ? 16 : 1;

  glm::vec3 lo(1e30f), hi(-1e30f);
  for (int s = 0; s < samples; s++) {
    skeleton.Update(length * s / samples);
    for (size_t m = 0; m < meshes.size(); m++) {
      glm::mat4 meshMat =
          skeleton.globalInverseTransform * skeleton.NodeGlobal(meshNodes[m]);
      for (const AnimatedVertex &v : meshes[m].getVertices()) {
        glm::mat4 skin(0.0f);
        float total = 0.0f;
        for (int k = 0; k < NUM_BONES_PER_VEREX; k++) {
          if (v.Weights[k] > 0.0f) {
            skin += skeleton.boneMats[v.BoneIDs[k]] * v.Weights[k];
            total += v.Weights[k];
          }
        }
        glm::vec3 p = glm::vec3((total > 0.0f ? skin : meshMat) *
                                glm::vec4(v.Position, 1.0f));
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
      }
    }
  }
  skeleton.Update(0.0);

  glm::vec3 size = hi - lo;
  float biggest = glm::max(size.x, glm::max(size.y, size.z));
  if (biggest > 0.0f) {
    fitScale = 1.8f / biggest;
    fitCenter = (lo + hi) * 0.5f;
    if (feetAtOrigin)
      fitCenter.y = lo.y;
  }
}

void AnimatedModel::useRealSize() {
  // The model is already in metres, standing on y = 0 around the origin
  fitScale = 1.0f;
  fitCenter = glm::vec3(0.0f);
}

void AnimatedModel::Draw(Shader *shader) {
  if (Gfx::headless)
    return;
  shader->setInt("skinned", 1);
  shader->setInt("idlePose", idle ? 1 : 0);
  shader->setVector3("fitCenter", fitCenter.x, fitCenter.y, fitCenter.z);
  shader->setFloat("fitScale", fitScale);
  glUniformMatrix4fv(glGetUniformLocation(shader->getID(), "gBones"),
                     skeleton.boneMats.size(), GL_FALSE,
                     glm::value_ptr(skeleton.boneMats[0]));
  for (unsigned int i = 0; i < meshes.size(); i++) {
    // Meshes without bones follow their own node instead.
    glm::mat4 meshMat =
        skeleton.globalInverseTransform * skeleton.NodeGlobal(meshNodes[i]);
    shader->setMatrix4("meshMat", glm::value_ptr(meshMat));
    meshes[i].Draw(shader);
  }
}
