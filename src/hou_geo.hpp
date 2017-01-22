
class cHouGeoAttrib {
public:
	enum eType {
		E_TYPE_fpreal32,
		E_TYPE_int32,
		E_TYPE_unknown,
	};

	std::string mName;
	eType mType = E_TYPE_unknown;
	int mAttribSize = 0;
	int mAttribCount = 0;
	std::unique_ptr<uint8_t[]> mpData;

public:
	void init(cstr name, cstr type, int attribSize, int count);

	float* get_float_val(int idx) {
		return &reinterpret_cast<float*>(mpData.get())[idx * mAttribSize];
	}
	int32_t* get_int32_val(int idx) {
		return &reinterpret_cast<int32_t*>(mpData.get())[idx * mAttribSize];
	}
	float const* get_float_val(int idx) const {
		return &reinterpret_cast<float const*>(mpData.get())[idx * mAttribSize];
	}
	int32_t const* get_int32_val(int idx) const {
		return &reinterpret_cast<int32_t const*>(mpData.get())[idx * mAttribSize];
	}
};

class cHouGeoGroup {
public:
	struct sInterval {
		int start;
		int end;

		int size() const { return end - start; }
	};

	std::string mName;
	std::unique_ptr<sInterval[]> mpIntervals;
	int mIntervalsCount = 0;
	bool mEmpty = true;
};

struct sHouGeoPrimPoly {
	int v[3];
	bool valid = false;
};

class cHouGeoLoader {
public:
	int mPointCount = 0;
	int mVertexCount = 0;
	int mPrimitiveCount = 0;
	int mGroupsCount = 0;
	int mNonemptyGroups = 0;
	int mPointAttribCount = 0;

	std::unique_ptr<cHouGeoAttrib[]> mpPointAttribs;
	std::unique_ptr<cHouGeoGroup[]> mpGroups;
	std::unique_ptr<int32_t[]> mpVertexMap;
	std::vector<sHouGeoPrimPoly> mPoly;
public:
	bool load(const fs::path& filepath);

protected:

	void check_nonempty_groups();
};
