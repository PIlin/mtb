#include <memory>
#include <vector>

#include "common.hpp"
#include "assimp_loader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>


static void list_meshes(aiScene const* pScene, aiNode const* pNode, std::vector<cAssimpLoader::sAIMeshInfo>& meshes) {
	if (pNode->mNumMeshes > 0) {
		meshes.push_back({ pScene->mMeshes[pNode->mMeshes[0]], pNode->mName.C_Str() });
	}
	for (uint32_t i = 0; i < pNode->mNumChildren; ++i) {
		list_meshes(pScene, pNode->mChildren[i], meshes);
	}
}


bool cAssimpLoader::load(cstr filepath, uint32_t flags) {
	auto pImporter = std::make_unique<Assimp::Importer>();
	pImporter->SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
		aiPrimitiveType_LINE | aiPrimitiveType_POINT | aiPrimitiveType_POLYGON);

	aiScene const* pScene = pImporter->ReadFile(filepath, flags);
	if (!pScene) {
		dbg_msg("cModelData::load_assimp(): assimp import error: %s\n", pImporter->GetErrorString());
		return false;
	}

	std::vector<sAIMeshInfo> meshes;
	meshes.reserve(pScene->mNumMeshes);
	list_meshes(pScene, pScene->mRootNode, meshes);

	mpImporter = std::move(pImporter);
	mpScene = pScene;
	mpMeshes = std::move(meshes);

	load_bones();

	return true;
}

bool cAssimpLoader::load(cstr filepath) {
	uint32_t flags = 0
		| aiProcess_FlipWindingOrder
		| aiProcess_SortByPType
		;

	return load(filepath, flags);
}

bool cAssimpLoader::load_unreal_fbx(cstr filepath) {
	uint32_t flags = 0
		| aiProcess_FlipWindingOrder
		| aiProcess_SortByPType
		| aiProcess_LimitBoneWeights
		;

	return load(filepath, flags);
}


void cAssimpLoader::load_bones() {
	std::unordered_map<cstr, int32_t> bonesMap;
	std::vector<sAIBoneInfo> bones;

	for (auto&& mi : mpMeshes) {
		aiMesh const* m = mi.mpMesh;
		if (!m->HasBones()) { continue; }

		for (uint32_t i = 0; i < m->mNumBones; ++i) {
			auto bone = m->mBones[i];
			auto name = bone->mName.C_Str();
			auto it = bonesMap.find(name);
			if (it == bonesMap.end()) {
				bonesMap[name] = (int32_t)bones.size();
				bones.emplace_back(sAIBoneInfo{ bone, name });
			}
		}
	}

	mpBones = std::move(bones);
	mBonesMap = std::move(bonesMap);
}
