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


namespace nModelLoader {
	ModelDataPtr find_or_load(const fs::path& path) {
		sResourceId id = sResourceId::make_id<cModelData>(path);

		cResourceMgr& resMgr = cResourceMgr::get();

		ModelDataPtr ptr = resMgr.find<cModelData>(id);
		if (ptr) return ptr;

		ptr = std::make_shared<cModelData>();
		if (!ptr->load(path)) return nullptr;
		
		resMgr.insert(id, ptr);

		return ptr;
	}

	bool find_or_load_unreal(const fs::path& path, ModelDataPtr& pOutMdlData, cRigData* pRigData) {
		sResourceId id = sResourceId::make_id<cModelData>(path);

		cResourceMgr& resMgr = cResourceMgr::get();

		ModelDataPtr ptr = resMgr.find<cModelData>(id);
		if (ptr)
		{
			assert(false); // todo: implement cRidData resource
			pOutMdlData = std::move(ptr);
			return false;
		}

		bool res = true;
		cAssimpLoader loader;
		res = res && loader.load_unreal_fbx(path);
		ptr = std::make_shared<cModelData>();
		res = res && ptr->load_assimp(loader);
		res = res && (!pRigData || pRigData->load(loader));

		if (res)
		{
			resMgr.insert(id, ptr);

			pOutMdlData = std::move(ptr);
			// todo: implement cRidData resource
		}
		return res;
	}

	bool find_or_load_unreal(const fs::path& path, ConstModelDataPtr& pOutMdlData, cRigData* pRigData) {
		ModelDataPtr pMdlData;
		bool res = find_or_load_unreal(path, *&pMdlData, pRigData);
		pOutMdlData = std::move(pMdlData);
		return res;
	}
}

