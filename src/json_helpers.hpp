#include <functional>

#include <cereal/external/rapidjson/document.h>

#define CHECK_SCHEMA(x, msg, ...) do { if (!(x)) { dbg_msg(msg, __VA_ARGS__); return false; } } while(0)

namespace nJsonHelpers {

using Document = ::CEREAL_RAPIDJSON_NAMESPACE::Document;
using Value = Document::ValueType;
using Size = ::CEREAL_RAPIDJSON_NAMESPACE::SizeType;

bool load_file(const fs::path& filepath, std::function<bool(Value const&)> loader);

} // namespace nJsonHelpers
