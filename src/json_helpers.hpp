#include <functional>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/filestream.h>

#define CHECK_SCHEMA(x, msg, ...) do { if (!(x)) { dbg_msg(msg, __VA_ARGS__); return false; } } while(0)

namespace nJsonHelpers {

using Document = rapidjson::Document;
using Value = Document::ValueType;
using Size = rapidjson::SizeType;

bool load_file(const fs::path& filepath, std::function<bool(Value const&)> loader);

} // namespace nJsonHelpers
