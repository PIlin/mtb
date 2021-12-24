#include "common.hpp"

#include "resource_mgr.hpp"
#include "math.hpp"
#include "rdr/gfx.hpp"
#include "rdr/rdr.hpp"
//#include "texture.hpp"
#include "rdr/model.hpp"
#include "rdr/rig.hpp"
#include "rdr/anim.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <assimp/Importer.hpp>
CLANG_DIAG_POP
#include "assimp_loader.hpp"


#include <entt/core/type_info.hpp>

#define IMPL_RES_TYPE_ID_FUNC(TYPE)  ResourceTypeId TYPE::type_id() { return entt::type_id<TYPE>().hash(); }
IMPL_RES_TYPE_ID_FUNC(cModelData)
IMPL_RES_TYPE_ID_FUNC(cModelMaterial)
IMPL_RES_TYPE_ID_FUNC(cRigData)
IMPL_RES_TYPE_ID_FUNC(cAnimationDataList)
IMPL_RES_TYPE_ID_FUNC(cAnimationList)
#undef IMPL_RES_TYPE_ID_FUNC


namespace nResLoader {

	template <typename TRes, typename ...TArgs>
	bool find_or_load_single(const ResourceNameId nameId, std::shared_ptr<const TRes>& pOut, TArgs... args) {
		sResourceId id = sResourceId::make_id<TRes>(nameId);
		cResourceMgr& resMgr = cResourceMgr::get();

		auto ptr = resMgr.find<TRes>(id);
		if (ptr) {
			pOut = std::move(ptr);
			return true;
		}

		ptr = std::make_shared<TRes>();
		if (ptr->load(std::forward<TArgs>(args)...)) {
			resMgr.insert(id, ptr);
			pOut = std::move(ptr);
			return true;
		}

		return false;
	}


	template <typename TRes>
	bool find_or_load_simple(const fs::path& path, std::shared_ptr<const TRes>& pOut) {
		return find_or_load_single(path, pOut, path);
	}

	bool find_or_load(const fs::path& path, ConstModelDataPtr& pOutMdlData) {
		return find_or_load_simple(path, pOutMdlData);
	}

	bool find_or_load(const fs::path& path, ConstRigDataPtr& pOutMdlData) {
		return find_or_load_simple(path, pOutMdlData);
	}

	bool find_or_load_unreal(const fs::path& path, ConstModelDataPtr& pOutMdlData, ConstRigDataPtr& pOutRigData) {
		sResourceId id = sResourceId::make_id<cModelData>(path);
		sResourceId rigId = sResourceId::make_id<cRigData>(path);

		cResourceMgr& resMgr = cResourceMgr::get();

		ModelDataPtr pMdl = resMgr.find<cModelData>(id);

		if (pMdl) {
			pOutMdlData = std::move(pMdl);
			pOutRigData = resMgr.find<cRigData>(rigId);
			return true;
		}

		bool res = true;
		cAssimpLoader loader;
		res = res && loader.load_unreal_fbx(path);
		pMdl = res ? std::make_shared<cModelData>() : nullptr;
		res = res && pMdl->load_assimp(loader);
		RigDataPtr pRig = res ? std::make_shared<cRigData>() : nullptr;
		res = res && pRig->load(loader);

		if (res) {
			resMgr.insert(id, pMdl);
			resMgr.insert(rigId, pRig);

			pOutMdlData = std::move(pMdl);
			pOutRigData = std::move(pRig);
			return true;
		}
		return false;
	}

	bool find_or_load(const fs::path& path, const ConstModelDataPtr& pMdlData, ConstModelMaterialPtr& pOutModelMaterial, bool isSkinnedByDef/* = false*/) {
		const ResourceNameId nameId = path / std::to_string(pMdlData->get_id_hash());

		ID3D11Device* pDev = get_gfx().get_dev();
		return find_or_load_single(nameId, pOutModelMaterial, pDev, pMdlData, path, isSkinnedByDef);
	}

	bool find_or_load(const fs::path& path, const fs::path& fname, ConstAnimationDataListPtr& pOutAnimDataList) {
		const ResourceNameId nameId = path / fname;
		return find_or_load_single(nameId, pOutAnimDataList, path, fname);
	}

	bool find_or_load_unreal(const fs::path& path, ConstAnimationDataListPtr& pOutAnimDataList) {
		sResourceId id = sResourceId::make_id<cAnimationDataList>(path);
		cResourceMgr& resMgr = cResourceMgr::get();
		AnimationDataListPtr ptr = resMgr.find<cAnimationDataList>(id);
		if (ptr) {
			pOutAnimDataList = std::move(ptr);
			return true;
		}

		cAssimpLoader animLoader;
		if (animLoader.load_unreal_fbx(path)) {
			ptr = std::make_shared<cAnimationDataList>();
			if (ptr->load(animLoader)) {
				resMgr.insert(id, ptr);
				pOutAnimDataList = std::move(ptr);
				return true;
			}
		}

		return false;
	}

	bool find_or_load(const fs::path& path, const fs::path& fname, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList) {
		const ResourceNameId nameId = (path / fname) / std::to_string(rigData.get_id_hash());

		sResourceId id = sResourceId::make_id<cAnimationList>(nameId);
		cResourceMgr& resMgr = cResourceMgr::get();
		AnimationListPtr ptr = resMgr.find<cAnimationList>(id);
		if (ptr) {
			pOutAnimList = std::move(ptr);
			return true;
		}

		ConstAnimationDataListPtr pAnimDataList;
		if (nResLoader::find_or_load(path, fname, *&pAnimDataList)) {
			ptr = std::make_shared<cAnimationList>();
			ptr->init(std::move(pAnimDataList), rigData);
			resMgr.insert(id, ptr);
			pOutAnimList = std::move(ptr);
			return true;
		}

		return false;
	}

	bool find_or_load_unreal(const fs::path& path, const cRigData& rigData, ConstAnimationListPtr& pOutAnimList) {
		const ResourceNameId nameId = path / std::to_string(rigData.get_id_hash());

		sResourceId id = sResourceId::make_id<cAnimationList>(nameId);
		cResourceMgr& resMgr = cResourceMgr::get();
		AnimationListPtr ptr = resMgr.find<cAnimationList>(id);
		if (ptr) {
			pOutAnimList = std::move(ptr);
			return true;
		}

		ConstAnimationDataListPtr pAnimDataList;
		if (nResLoader::find_or_load_unreal(path, *&pAnimDataList)) {
			ptr = std::make_shared<cAnimationList>();
			ptr->init(std::move(pAnimDataList), rigData);
			resMgr.insert(id, ptr);
			pOutAnimList = std::move(ptr);
			return true;
		}

		return false;
	}
}

