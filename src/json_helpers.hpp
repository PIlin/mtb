#include <functional>

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wunused-local-typedef")
#include <cereal/external/rapidjson/document.h>
CLANG_DIAG_POP

#define CHECK_SCHEMA(x, msg, ...) do { if (!(x)) { dbg_msg(msg, __VA_ARGS__); return false; } } while(0)

namespace nJsonHelpers {

using Document = ::CEREAL_RAPIDJSON_NAMESPACE::Document;
using Value = Document::ValueType;
using Size = ::CEREAL_RAPIDJSON_NAMESPACE::SizeType;

bool load_file(const fs::path& filepath, std::function<bool(Value const&)> loader);

} // namespace nJsonHelpers
