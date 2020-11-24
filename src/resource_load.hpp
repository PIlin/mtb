#pragma once

#include <memory>

#include "path_helpers.hpp"

#define FRWD_RES(TYPE, NAME) \
	class TYPE; \
	using Const ## NAME = std::shared_ptr<const TYPE>

FRWD_RES(cModelData, ModelDataPtr);
FRWD_RES(cRigData, RigDataPtr);
FRWD_RES(cAnimationDataList, AnimationDataListPtr);
FRWD_RES(cAnimationList, AnimationListPtr);

#undef FRWD_RES

class cRigData;
namespace nResLoader {
	bool find_or_load(const fs::path& path, ConstModelDataPtr& pOutMdlData);
	bool find_or_load(const fs::path& path, ConstRigDataPtr& pOutRigData);

	bool find_or_load_unreal(const fs::path& path, ConstModelDataPtr& pOutMdlData, ConstRigDataPtr& pOutRigData);

	bool find_or_load(const fs::path& path, const fs::path& fname, ConstAnimationDataListPtr& pOutAnimDataList);
	bool find_or_load_unreal(const fs::path& path, ConstAnimationDataListPtr& pOutAnimDataList);

	bool find_or_load(const fs::path& path, const fs::path& fname, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList);
	bool find_or_load_unreal(const fs::path& path, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList);
}
