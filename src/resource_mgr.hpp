#pragma once

#include "resource.hpp"
#include "path_helpers.hpp"

#include <cassert>
#include <map>
#include <string>
#include <unordered_map>

using ResourceNameId = fs::path;

struct sResourceId {
	ResourceTypeId typeId;
	ResourceNameId name;

	sResourceId(ResourceTypeId typeId, const ResourceNameId& name)
		: typeId(typeId)
		, name(name)
	{}

	sResourceId(ResourceTypeId typeId, const cstr name)
		: typeId(typeId)
		, name(name.p)
	{}

	template <typename TResource>
	static sResourceId make_id(const cstr name) {
		return sResourceId(TResource::type_id(), name);
	}

	template <typename TResource>
	static sResourceId make_id(const ResourceNameId& name) {
		return sResourceId(TResource::type_id(), name);
	}
};



class cResourceMgr : noncopyable {
	using ResourceMap = std::map<ResourceNameId, ResourceBasePtr>;
	using TypeResourceMap = std::unordered_map<ResourceTypeId, ResourceMap>;

	TypeResourceMap mResMap;

public:
	static cResourceMgr& get();

	~cResourceMgr();

	void insert(const sResourceId& id, ResourceBasePtr ptr);

	ResourceBasePtr find_base(const sResourceId& id) const;

	template <typename TResource>
	std::shared_ptr<TResource> find(const sResourceId& id) const {
		assert(TResource::type_id() == id.typeId);
		return std::static_pointer_cast<TResource>(find_base(id));
	}

	template <typename TResource>
	std::shared_ptr<TResource> find(const ResourceNameId& nameId) const {
		return find<TResource>(sResourceId::make_id<TResource>(nameId));
	}

private:
	void clear();
};

