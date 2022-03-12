#include "mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "structures.h"

Mesh::Mesh() {
  static Vertex vertices[3] = {
      {glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
       glm::normalize(glm::vec3(0.5f, 1.0f, 0.0f)), glm::vec2(0.0f, 0.0f)},
      {glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
       glm::normalize(glm::vec3(-0.5f, -1.0f, 0.0f)), glm::vec2(1.0f, 0.0f)},
      {glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
       glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)), glm::vec2(0.0f, 1.0f)}};
  vertex_count_ = 3;
  vertex_buffer_ = ResourceManager::Get()->CreateDeviceBufferWithData(
      vk::BufferUsageFlagBits::eVertexBuffer, (const void *)&vertices,
      sizeof(vertices));
}

Mesh::Mesh(const std::string &filename) {
  std::vector<Vertex> vertices;

  Assimp::Importer importer;

  const aiScene *scene = importer.ReadFile(
      filename, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_MakeLeftHanded);

  if (scene->mNumMeshes < 1) {
    throw "No meshes in the file.";
  }
  aiMesh *mesh = scene->mMeshes[0];
  if (!(mesh->HasFaces() && mesh->HasPositions() && mesh->HasNormals() &&
        mesh->HasTextureCoords(0) && mesh->HasTangentsAndBitangents())) {
    throw "Mesh doesn't have some required data.";
  }

  for (size_t face_idx = 0; face_idx < mesh->mNumFaces; face_idx++) {
    aiFace &face = mesh->mFaces[face_idx];
    if (face.mNumIndices != 3)
      throw "Not a triangle!";

    for (size_t i = 0; i < 3; i++) {
      auto idx = face.mIndices[i];
      aiVector3D pos = mesh->mVertices[idx];
      aiVector3D normal = mesh->mNormals[idx];
      aiVector3D tangent = mesh->mTangents[idx];
      aiVector3D uv = mesh->mTextureCoords[0][idx];
      vertices.push_back(Vertex{
          glm::vec3(pos.x, pos.y, pos.z),
          glm::vec3(normal.x, normal.y, normal.z),
          glm::vec3(tangent.x, tangent.y, tangent.z),
          glm::vec2(uv.x, uv.y),
      });
    }
  }

  vertex_count_ = vertices.size();
  vertex_buffer_ = ResourceManager::Get()->CreateDeviceBufferWithData(
      vk::BufferUsageFlagBits::eVertexBuffer, vertices.data(),
      sizeof(Vertex) * vertices.size());
}

Mesh::~Mesh() {}
