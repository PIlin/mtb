#pragma once

#include <array>
#include <functional>
#include <vector>

class cUpdateRecord;

#define USE_GRAPH_UPDATE (1)

class cUpdateSubscriberScope : noncopyable {
	cUpdateRecord* mpUpdateRecord = nullptr;
public:

	cUpdateSubscriberScope() = default;
	~cUpdateSubscriberScope() { reset(); }
	void reset();

	operator bool() const { return mpUpdateRecord != nullptr; }

private:
	friend class cUpdateRecord;
	void set_record(cUpdateRecord* pUpdateRecord) { mpUpdateRecord = pUpdateRecord; }
	void unsubscribed();
};

enum class eUpdatePriority : int32_t {
	First = -1,
	Begin = 0,
	//Input,
	Camera,
	Light,
	SceneAnimUpdate,
	SceneDisp,
	End,
	Count,
	Last = Count
};

using tUpdateFunc = std::function<void()>;

class cUpdateRecord : noncopyable {
protected:
	tUpdateFunc mFunc;
	cUpdateSubscriberScope* mpScope = nullptr;
	eUpdatePriority mPriority = eUpdatePriority::Last;
public:
	cUpdateRecord() = default;
	cUpdateRecord(const eUpdatePriority priority, tUpdateFunc&& func, cUpdateSubscriberScope& scope);
	cUpdateRecord(cUpdateRecord&& other) {
		mFunc = std::move(other.mFunc);
		mpScope = std::move(other.mpScope);
		mPriority = std::move(other.mPriority);
		other.invalidate();
		if (mpScope) { mpScope->set_record(this); }
	}
	cUpdateRecord& operator=(cUpdateRecord&& other) {
		mFunc = std::move(other.mFunc);
		mpScope = std::move(other.mpScope);
		mPriority = std::move(other.mPriority);
		other.invalidate();
		if (mpScope) { mpScope->set_record(this); }
		return *this;
	}
	~cUpdateRecord();

	eUpdatePriority get_priority() const { return mPriority; }
	bool is_valid() const { return mPriority != eUpdatePriority::Last; }

	void invalidate();

	bool exec();
	//bool operator<(const cUpdateRecord& other);
};

class cUpdateQueue {
	using tRecords = std::vector<cUpdateRecord>;

	struct sBucket {
		tRecords mRecords;
		enum eFlags : uint32_t {
			eDirty = 1 << 0,
			eInUse = 1 << 1
		} mFlags;

		void exec();
	};

	std::array<sBucket, (uint32_t)eUpdatePriority::Count> mBuckets;
	eUpdatePriority mExecutedPriority = eUpdatePriority::Last;
	tRecords mTempRecords;
public:

	void add(const eUpdatePriority priority, tUpdateFunc&& func, cUpdateSubscriberScope& scope);

	void begin_exec();
	void advance_exec(const eUpdatePriority targetPriority);
	void end_exec();

private:

	void commit_temp_records();
};




struct sUpdateDepRes {
	uint32_t hash;
	cstr name;

	sUpdateDepRes(uint32_t hash, cstr name) 
		: hash(hash)
		, name(name)
	{}
};

struct sUpdateDepDesc {
	std::vector<sUpdateDepRes> inputs;
	std::vector<sUpdateDepRes> outputs;
};

class cUpdateGraphNode : protected cUpdateRecord {
public:
	using NodeId = uint32_t;
	static constexpr NodeId InvalidId = ~NodeId(0);

private:
	sUpdateDepDesc mDesc;
	NodeId mId = InvalidId;

public:
	cUpdateGraphNode() = default;
	cUpdateGraphNode(NodeId id, const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope);
	cUpdateGraphNode(cUpdateGraphNode&& other)
		: cUpdateRecord(std::move(other))
		, mDesc(std::move(other.mDesc))
		, mId(other.mId)
	{}

	cUpdateGraphNode& operator=(cUpdateGraphNode&& other) {
		cUpdateRecord::operator=(std::move(other));
		mDesc = std::move(other.mDesc);
		mId = other.mId;
		other.mId = InvalidId;
		return *this;
	}
	~cUpdateGraphNode() = default;

	NodeId get_id() const { return mId; }
	const sUpdateDepDesc& get_deps() const { return mDesc; }
	using cUpdateRecord::exec;
};

class cUpdateGraph {
	using NodeId = cUpdateGraphNode::NodeId;
	using tNodes = std::unordered_map<NodeId, cUpdateGraphNode>;
	using tAdjList = std::unordered_map<NodeId, std::vector<NodeId>>; // nodeid -> (nodeId,...)
	using tNoInputList = std::vector<NodeId>;
	using tPendingNodes = std::vector<cUpdateGraphNode>;
	using tOrder = std::vector<NodeId>;

	struct sResource {
		uint32_t hash;
		std::string name;
	};
	using tResourceMap = std::unordered_map<uint32_t, sResource>;

	struct sTopoSorter;
	struct sTopoExecutor;
	struct sTaskExecutor;

public:
	void add(const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope);
	void exec();

	sUpdateDepRes register_res(cstr name);

private:
	void commit_pending();
	void update_dirty();
	void build_adj_list();
	void topo_sort();

	void exec_node(NodeId id);

	using tEdgeMap = std::unordered_map<uint32_t, std::vector<NodeId>>;
	static void build_adj_list(const tEdgeMap& inEdgeMap, const tEdgeMap& outEdgeMap, tAdjList& adjList);
private:
	tNodes mNodes;
	tAdjList mAdjList;
	tAdjList mRevAdjList;
	tNoInputList mNoInputList;
	tOrder mOrder;
	tPendingNodes mPending;
	tResourceMap mResources;
	bool mIsDirty = false;
	NodeId mIdGen = 0;
};

