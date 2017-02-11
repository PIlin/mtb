class cAssimpLoader;
class cRdrContext;

struct sJointData {
	int idx;
	int parIdx;
	int skinIdx;
};

class cRigData : public noncopyable {
	int mJointsNum = 0;
	int mIMtxNum = 0;
	std::unique_ptr<sJointData[]> mpJoints;
	std::unique_ptr<DirectX::XMMATRIX[]> mpLMtx;
	std::unique_ptr<DirectX::XMMATRIX[]> mpIMtx;
	std::unique_ptr<std::string[]> mpNames;

public:
	bool load(const fs::path& filepath);
	bool load(cAssimpLoader& loader);
	
	int find_joint_idx(cstr name) const;
private:

	bool load_json(const fs::path& filepath);

	friend class cJsonLoaderImpl;
	friend class cRig;
};



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
	void set_parent_mtx(DirectX::XMMATRIX* pMtx) { mpParentMtx = pMtx; }

	sXform& get_xform() { return *mpXform; }

	void calc_local();
	void calc_world();

private:
	friend class cRig;
};

class cRig {
	int mJointsNum = 0;
	std::unique_ptr<cJoint[]> mpJoints;
	cRigData const* mpRigData = nullptr;
	std::unique_ptr<DirectX::XMMATRIX[]> mpLMtx;
	std::unique_ptr<DirectX::XMMATRIX[]> mpWmtx;
	std::unique_ptr<sXform[]> mpXforms;
public:

	void init(cRigData const* pRigData);

	void calc_local();
	void calc_world();

	void upload_skin(cRdrContext const& rdrCtx) const;

	cJoint* get_joint(int idx) const;
	cJoint* find_joint(cstr name) const;

};

