#include "common.hpp"
#include "res/path_helpers.hpp"
#include "json_helpers.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <SDL_rwops.h>
CLANG_DIAG_POP

#include <memory>

using namespace nJsonHelpers;

bool nJsonHelpers::load_file(const fs::path& filepath, std::function<bool(Value const&)> loader) {
	sInputFile file(filepath);
	if (!file.is_open()) { return false; }

	const size_t size = file.mSize;
	auto pdata = std::make_unique<char[]>(size + 1);
	char* json = pdata.get();
	file.read_buffer(json, size);
	json[size] = '\0';
	
	Document document;
	if (document.ParseInsitu<0>(json).HasParseError()) {
		dbg_msg("nJsonHelpers::load_file(): error parsing json file <%s>: %s\n%d", filepath.c_str(), document.GetParseError());
		return false;
	}

	return loader(document);
}
