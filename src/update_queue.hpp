#pragma once

#include "microprofile.h"

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

struct tUpdateFunc {
	using tFunc = std::function<void()>;
	tFunc mFunc;
	cstr mDbgName;
#if 1 == MICROPROFILE_ENABLED
	MicroProfileToken mDbgProfileToken = MICROPROFILE_INVALID_TOKEN;
#endif

	tUpdateFunc() = default;
	tUpdateFunc(tFunc&& f, cstr name)
		: mFunc(std::move(f))
		, mDbgName(name)
#if 1 == MICROPROFILE_ENABLED
		, mDbgProfileToken(MicroProfileGetToken("update", name, -1))
#endif
	{}

	void operator()() const { 
#if 1 == MICROPROFILE_ENABLED
		MicroProfileScopeHandler scope(mDbgProfileToken);
#endif
		mFunc(); 
	}

	operator bool() const { return static_cast<bool>(mFunc); }

	cstr get_dbg_name() const { return mDbgName; }
};

#define MAKE_UPDATE_FUNC_THIS(MEMB) tUpdateFunc(std::bind(&MEMB, this), #MEMB)

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

	bool exec() const;
	//bool operator<(const cUpdateRecord& other);
	cstr get_dbg_name() const { return mFunc.get_dbg_name(); }
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
	uint32_t hashGroup;
	uint32_t hashType;
	cstr nameGroup;
	cstr nameType;

	sUpdateDepRes(uint32_t hashGroup, cstr nameGroup, uint32_t hashType, cstr nameType) 
		: hashGroup(hashGroup)
		, hashType(hashType)
		, nameGroup(nameGroup)
		, nameType(nameType)
	{}

	sUpdateDepRes get_group() const { return sUpdateDepRes(hashGroup, nameGroup, 0, nullptr); }
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
	using cUpdateRecord::get_dbg_name;
};

class cUpdateGraph {
	using NodeId = cUpdateGraphNode::NodeId;
	using tNodes = std::unordered_map<NodeId, cUpdateGraphNode>;
	using tAdjList = std::unordered_map<NodeId, std::vector<NodeId>>; // nodeid -> (nodeId,...)
	using tNoInputList = std::vector<NodeId>;
	using tPendingNodes = std::vector<cUpdateGraphNode>;
	using tOrder = std::vector<NodeId>;

	struct sResourceHash {
		uint32_t group;
		uint32_t type;

		sResourceHash() = default;
		sResourceHash(uint32_t group, uint32_t type)
			: group(group)
			, type(type)
		{}

		sResourceHash(const sUpdateDepRes& res)
			: group(res.hashGroup)
			, type(res.hashType)
		{}

		bool operator==(const sResourceHash& other) const { return group == other.group && type == other.type; }
		bool operator!=(const sResourceHash& other) const { return !(*this == other); }

		sResourceHash get_group() const { return sResourceHash{ group, 0 }; }

		struct hash
		{
			std::size_t operator()(sResourceHash const& h) const noexcept
			{
				const uint64_t value = uint64_t(h.group) | uint64_t(h.type) << 32;
				return std::hash<uint64_t>{}(value);
			}
		};
	};

	struct sResource {
		sResourceHash hash;
		std::string nameGroup;
		std::string nameType;
	};

	using tResourceMap = std::unordered_map<sResourceHash, sResource, sResourceHash::hash>;

	struct sTopoSorter;
	struct sTopoExecutor;
	struct sTaskExecutor;

public:
	void add(const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope);
	void exec();

	sUpdateDepRes register_res(cstr nameGroup, cstr nameType);

private:
	sUpdateDepRes register_res_impl(cstr nameGroup, cstr nameType);

	void commit_pending();
	void update_dirty();
	void build_adj_list();
	void topo_sort();

	void exec_node(NodeId id);

	bool dbg_on_exec_node(cUpdateGraphNode const& node);

	using tEdgeMap = std::unordered_map<sResourceHash, std::vector<NodeId>, sResourceHash::hash>;
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
	bool mIsOrderDirty = false;
	NodeId mIdGen = 0;

	std::unordered_map<sResourceHash, bool, sResourceHash::hash> mDbgResourceHighlight;
	std::unordered_map<NodeId, bool> mDbgNodeState;
};

