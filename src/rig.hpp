struct ID3D11DeviceContext;


struct sJointData {
	int idx;
	int parIdx;
	int skinIdx;
};

class cRigData : public noncopyable {
	int mJointsNum = 0;
	int mIMtxNum = 0;
	sJointData* mpJoints = nullptr;
	DirectX::XMMATRIX* mpLMtx = nullptr;
	DirectX::XMMATRIX* mpIMtx = nullptr;
	std::string* mpNames = nullptr;
	bool mAllocatedArrays = false;

public:
	//cRigData() {}
	~cRigData();

	bool load(cstr filepath);
	
	int find_joint_idx(cstr name) const;
private:

	bool load_json(cstr filepath);

	friend class cJsonLoaderImpl;
	friend class cRig;
};



class cJoint {
	sXform* mpXform = nullptr;
	DirectX::XMMATRIX* mpLMtx = nullptr;
	DirectX::XMMATRIX* mpWMtx = nullptr;
	DirectX::XMMATRIX const* mpIMtx = nullptr;
	DirectX::XMMATRIX const* mpParentMtx = nullptr;
	//cJoint* mpParent = nullptr;
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
	int mJointsNum;
	cJoint* mpJoints;
	cRigData const* mpRigData;
	DirectX::XMMATRIX* mpLMtx;
	DirectX::XMMATRIX* mpWmtx;
	sXform* mpXforms;
public:

	void init(cRigData const* pRigData);

	void calc_local();
	void calc_world();

	void upload_skin(ID3D11DeviceContext* pCtx);

	cJoint* get_joint(int idx) const;
	cJoint* find_joint(cstr name) const;

};

