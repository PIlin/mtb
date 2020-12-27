#include <vector>
#include <unordered_map>

struct aiScene;
struct aiMesh;
struct aiBone;
namespace Assimp {
	class Importer;
}

class cAssimpLoader {
public:
	struct sAIMeshInfo {
		aiMesh* mpMesh;
		cstr mName;
	};

	struct sAIBoneInfo {
		aiBone* mpBone;
		cstr mName;
	};

private:
	std::unique_ptr<Assimp::Importer> mpImporter;
	aiScene const* mpScene = nullptr;

	std::vector<sAIMeshInfo> mpMeshes;
	std::vector<sAIBoneInfo> mpBones;
	std::unordered_map<cstr, int32_t> mBonesMap;
public:
	cAssimpLoader();
	~cAssimpLoader();

	bool load(const fs::path& filepath, uint32_t flags);
	bool load(const fs::path& filepath);
	bool load_unreal_fbx(const fs::path& filepath);

	aiScene const* get_scene() const { return mpScene; }
	std::vector<sAIMeshInfo> const& get_mesh_info() const { return mpMeshes; }
	std::vector<sAIBoneInfo> const& get_bones_info() const { return mpBones; }
	std::unordered_map<cstr, int32_t> const& get_bones_map() const { return mBonesMap; }

private:

	void load_bones();
};
