#pragma once

#include <memory>


using ResourceTypeId = uint32_t;

struct sResourceBase : public std::enable_shared_from_this<sResourceBase>, noncopyable {

};

using ResourceBasePtr = std::shared_ptr<sResourceBase>;

#define DEF_RES_PTR(TYPE, NAME) \
	using NAME = std::shared_ptr<TYPE>; \
	using Const ## NAME = std::shared_ptr<const TYPE>;
