#include "resource.hpp"

class cAssimpLoader;
class cRdrContext;

struct sJointData {
	int idx;
	int parIdx;
	int skinIdx;
};

class cRigData : public sResourceBase{
	int mJointsNum = 0;
	int mIMtxNum = 0;
	std::unique_ptr<sJointData[]> mpJoints;
	std::unique_ptr<DirectX::XMMATRIX[]> mpLMtx;
	std::unique_ptr<DirectX::XMMATRIX[]> mpIMtx;
	std::unique_ptr<std::string[]> mpNames;

public:
	static ResourceTypeId type_id();

	bool load(const fs::path& filepath);
	bool load(cAssimpLoader& loader);
	
	int find_joint_idx(cstr name) const;
private:

	bool load_json(const fs::path& filepath);

	friend class cJsonLoaderImpl;
	friend class cRig;
};

DEF_RES_PTR(cRigData, RigDataPtr)

class cJoint {
	sXform* mpXform = nullptr;
	DirectX::XMMATRIX* mpLMtx = nullptr;
	DirectX::XMMATRIX* mpWMtx = nullptr;
	DirectX::XMMATRIX const* mpIMtx = nullptr;
	DirectX::XMMATRIX const* mpParentMtx = nullptr;

public:
	DirectX::XMMATRIX& get_local_mtx() { return *mpLMtx; }
	DirectX::XMMATRIX& get_world_mtx() { return *mpWMtx; }
	DirectX::XMMATRIX const* get_inv_mtx() { return mpIMtx; }
	//void set_inv_mtx(DirectX::XMMATRIX* pMtx) { mpIMtx = pMtx; }
	

	sXform& get_xform() { return *mpXform; }

	void calc_local();
	void calc_world();

private:
	void set_parent_mtx(DirectX::XMMATRIX* pMtx) { mpParentMtx = pMtx; }

	friend class cRig;
};

class cRig {
	int mJointsNum = 0;
	std::unique_ptr<cJoint[]> mpJoints;
	ConstRigDataPtr mpRigData = nullptr;
	std::unique_ptr<DirectX::XMMATRIX[]> mpLMtx; // jointsNum 
	std::unique_ptr<DirectX::XMMATRIX[]> mpWmtx; // jointsNum + 1 for joint 0 parent
	std::unique_ptr<sXform[]> mpXforms;          // jointsNum 
public:

	void init(ConstRigDataPtr pRigData);

	void calc_local();
	void calc_world(DirectX::XMMATRIX rootWMtx);

	void upload_skin(cRdrContext const& rdrCtx) const;

	cJoint* get_joint(int idx) const;
	cJoint* find_joint(cstr name) const;
	cRigData const* get_rig_data() const;
};

