#include "common.hpp"

#include "resource_mgr.hpp"
#include "math.hpp"
#include "rdr.hpp"
//#include "texture.hpp"
#include "model.hpp"
#include "rig.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <assimp/Importer.hpp>
CLANG_DIAG_POP
#include "assimp_loader.hpp"


#include <entt/core/type_info.hpp>

ResourceTypeId cModelData::type_id() { return entt::type_info<cModelData>::id(); }
ResourceTypeId cRigData::type_id() { return entt::type_info<cRigData>::id(); }


namespace nResLoader {

	template <typename TRes>
	bool find_or_load_simple(const fs::path& path, std::shared_ptr<const TRes>& pOut) {
		sResourceId id = sResourceId::make_id<TRes>(path);
		cResourceMgr& resMgr = cResourceMgr::get();

		auto ptr = resMgr.find<TRes>(id);
		if (ptr)
		{
			pOut = std::move(ptr);
			return true;
		}

		ptr = std::make_shared<TRes>();
		if (ptr->load(path))
		{
			resMgr.insert(id, ptr);
			pOut = std::move(ptr);
			return true;
		}

		return false;
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

		if (pMdl)
		{
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

		if (res)
		{
			resMgr.insert(id, pMdl);
			resMgr.insert(rigId, pRig);

			pOutMdlData = std::move(pMdl);
			pOutRigData = std::move(pRig);
			return true;
		}
		return false;
	}

}

