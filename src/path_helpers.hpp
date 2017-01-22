#pragma once

#include <experimental/filesystem>
#include <fstream>

namespace fs = std::experimental::filesystem;


class cPathManager {
	fs::path mBasePath;
	fs::path mDataPath;
public:
	static cPathManager& get();

	cPathManager();

	const fs::path& get_base_path() const { return mBasePath; }
	const fs::path& get_data_path() const { return mDataPath; }


	static fs::path build_data_path(const fs::path& path) {
		return cPathManager::get().get_data_path() / path;
	}
};


struct sInputFile {
	std::ifstream mStream;
	size_t mSize = 0;

	sInputFile() = default;
	sInputFile(const fs::path& path);
	sInputFile(const fs::path& path, const fs::path& basePath);

	bool open(const fs::path& path);
	bool open(const fs::path& path, const fs::path& basePath);

	std::unique_ptr<char[]> read_all();
	void read_buffer(void* buf, size_t bufSize);

	bool is_open() const { return mStream.is_open(); }

	static sInputFile open_data_file(const fs::path& path) {
		return sInputFile(path, cPathManager::get().get_data_path());
	}
};

#define PRI_FILE "ls"