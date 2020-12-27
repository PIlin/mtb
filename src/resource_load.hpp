#pragma once

#include <memory>

#include "path_helpers.hpp"

#define FRWD_RES(TYPE, NAME) \
	class TYPE; \
	using Const ## NAME = std::shared_ptr<const TYPE>

FRWD_RES(cModelData, ModelDataPtr);
FRWD_RES(cModelMaterial, ModelMaterialPtr);
FRWD_RES(cRigData, RigDataPtr);
FRWD_RES(cAnimationDataList, AnimationDataListPtr);
FRWD_RES(cAnimationList, AnimationListPtr);

#undef FRWD_RES

class cRigData;
namespace nResLoader {
	// model and rig
	bool find_or_load(const fs::path& path, ConstModelDataPtr& pOutMdlData);
	bool find_or_load(const fs::path& path, ConstRigDataPtr& pOutRigData);

	// model and rig (fbx)
	bool find_or_load_unreal(const fs::path& path, ConstModelDataPtr& pOutMdlData, ConstRigDataPtr& pOutRigData);

	// model material
	bool find_or_load(const fs::path& path, const ConstModelDataPtr& pMdlData, ConstModelMaterialPtr& pOutModelMaterial, bool isSkinnedByDef = false);

	// animation data
	bool find_or_load(const fs::path& path, const fs::path& fname, ConstAnimationDataListPtr& pOutAnimDataList);
	bool find_or_load_unreal(const fs::path& path, ConstAnimationDataListPtr& pOutAnimDataList);

	// animation for rig
	bool find_or_load(const fs::path& path, const fs::path& fname, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList);
	bool find_or_load_unreal(const fs::path& path, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList);
}
