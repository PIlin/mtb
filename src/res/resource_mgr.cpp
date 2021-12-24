#include "common.hpp"
#include "resource_mgr.hpp"

#include <cassert>

cResourceMgr::~cResourceMgr() {
	clear();
}


ResourceBasePtr cResourceMgr::find_base(const sResourceId& id) const {
	TypeResourceMap::const_iterator tit = mResMap.find(id.typeId);
	if (tit == mResMap.end()) return nullptr;

	const ResourceMap& rm = tit->second;
	ResourceMap::const_iterator rit = rm.find(id.name);
	if (rit == rm.end()) return nullptr;

	return rit->second;
}


void cResourceMgr::insert(const sResourceId& id, ResourceBasePtr ptr) {
	assert(ptr);
	if (!ptr) return; // todo: remember failed resources?

	const ResourceIdHash hash = hash_fnv_1a_cwstr(id.name.c_str());
	ptr->set_id_hash(hash);

	mResMap[id.typeId][id.name] = std::move(ptr);
}



template <typename UM, typename PRED>
size_t erase_if(UM& c, PRED& pred) {
	// see https://en.cppreference.com/w/cpp/container/unordered_map/erase_if
	auto old_size = c.size();
	for (auto i = c.begin(), last = c.end(); i != last; ) {
		if (pred(*i)) {
			i = c.erase(i);
		}
		else {
			++i;
		}
	}
	return old_size - c.size();
}


void cResourceMgr::clear() {
	bool anyCleared = false;
	do {
		for (auto& trPair : mResMap) {
			ResourceMap& rm = trPair.second;
			if (erase_if(rm, [](const ResourceMap::value_type& item) { return item.second.use_count() == 1; }) > 0) {
				anyCleared = true;
			}
		}
		if (erase_if(mResMap, [](const TypeResourceMap::value_type& item) { return item.second.empty(); }) > 0) {
			anyCleared = true;
		}
	} while (anyCleared && !mResMap.empty());
	
	if (!mResMap.empty()) {
		assert(mResMap.empty());
		for (const auto& trPair : mResMap) {
			const ResourceMap& rm = trPair.second;
			for (const auto& idResPair : rm) {
				dbg_msg("leaked type %x, id %" PRI_FILE ", refcount %zu\n", trPair.first, idResPair.first.c_str(), idResPair.second.use_count());
			}
		}
		mResMap.clear();
	}
}
