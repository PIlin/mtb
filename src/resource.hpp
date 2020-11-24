#pragma once

#include <memory>


using ResourceTypeId = uint32_t;
using ResourceIdHash = uint32_t;

struct sResourceBase : public std::enable_shared_from_this<sResourceBase>, noncopyable {
protected:
	ResourceIdHash mIdHash = 0;
public:
	ResourceIdHash get_id_hash() const { return mIdHash; }
protected:
	virtual ~sResourceBase() {}
	void set_id_hash(ResourceIdHash idHash) { mIdHash = idHash; }
	friend class cResourceMgr;
};

using ResourceBasePtr = std::shared_ptr<sResourceBase>;

#define DEF_RES_PTR(TYPE, NAME) \
	using NAME = std::shared_ptr<TYPE>; \
	using Const ## NAME = std::shared_ptr<const TYPE>;
