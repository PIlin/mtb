#include "path_helpers.hpp"

#include "common.hpp"

#include <SDL_filesystem.h>

// TODO: move somewhere
struct sdl_ptr_releaser {
	template <typename T>
	void operator()(T* p) {
		SDL_free(p);
	}
};

template <typename T>
using sdl_ptr = base_ptr<T, sdl_ptr_releaser>;


static fs::path obtain_base_path() {
	sdl_ptr<char> szPath(SDL_GetBasePath());
	return fs::u8path(szPath.p);
}


cPathManager::cPathManager() 
	: mBasePath(obtain_base_path())
	, mDataPath(fs::absolute("../data/", mBasePath))
{}




sInputFile::sInputFile(const fs::path& path) {
	open(path);
}

sInputFile::sInputFile(const fs::path& path, const fs::path& basePath) {
	open(path, basePath);
}

bool sInputFile::open(const fs::path& path) {
	mStream = std::ifstream(path, std::ifstream::ate | std::ifstream::binary);
	if (mStream.is_open()) {
		mSize = mStream.tellg();
		mStream.seekg(0, std::ifstream::beg);
		return true;
	}
	return false;
}

bool sInputFile::open(const fs::path& path, const fs::path& basePath) {
	return open(basePath / path);
}

std::unique_ptr<char[]> sInputFile::read_all() {
	if (!is_open()) { return std::unique_ptr<char[]>(); }

	auto buf = std::make_unique<char[]>(mSize);

	mStream.seekg(0, std::ifstream::beg);
	mStream.read(buf.get(), mSize);

	return std::move(buf);
}

void sInputFile::read_buffer(void* buf, size_t bufSize) {
	if (!is_open()) { return; }
	mStream.read((char*)buf, bufSize);
}
