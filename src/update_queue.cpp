#include "common.hpp"
#include "update_queue.hpp"

#include <cassert>
#include <queue>

//////

void cUpdateSubscriberScope::reset() {
	if (mpUpdateRecord) {
		mpUpdateRecord->invalidate();
		mpUpdateRecord = nullptr;
	}
}

void cUpdateSubscriberScope::unsubscribed() {
	mpUpdateRecord = nullptr;
}

//////


cUpdateRecord::cUpdateRecord(const eUpdatePriority priority, tUpdateFunc&& func, cUpdateSubscriberScope& scope)
	: mFunc(std::move(func))
	, mpScope(&scope)
	, mPriority(priority)
{
	scope.set_record(this);
}

cUpdateRecord::~cUpdateRecord() {
	if (is_valid()) {
		assert(mpScope);
		mpScope->unsubscribed();
	}
}

bool cUpdateRecord::exec() {
	if (is_valid()) {
		assert(mFunc);
		mFunc();
	}
	return is_valid();
}

void cUpdateRecord::invalidate() {
	mFunc = tUpdateFunc();
	mpScope = nullptr;
	mPriority = eUpdatePriority::Last;
}

//////



void cUpdateQueue::sBucket::exec() {
	auto flagScope = sScopedExecutor::create(
		[this] { nFlag::set(mFlags, eInUse); },
		[this] { nFlag::reset(mFlags, eInUse); }
	);

	if (nFlag::check(mFlags, eDirty)) {
		//std::sort(mRecords.begin(), mRecords.end());
		//std::stable_sort(mRecords.begin(), mRecords.end());
		//auto iter = std::lower_bound(mRecords.begin(), mRecords.end(), cUpdateRecord(), 
		//	[](const cUpdateRecord& left, const cUpdateRecord& right) {
		//		return (left.get_priority() < eUpdatePriority::Last);
		//	});

		auto iter = std::partition(mRecords.begin(), mRecords.end(), [](const cUpdateRecord& rec) { return (rec.get_priority() < eUpdatePriority::Last); });
		mRecords.erase(iter, mRecords.end());
		nFlag::reset(mFlags, eDirty);
	}

	bool bDirty = false;
	for (cUpdateRecord& record : mRecords) {
		const bool bValid = record.exec();
		bDirty = bDirty || (!bValid);
	}

	if (bDirty) {
		nFlag::set(mFlags, eDirty);
	}
}


void cUpdateQueue::add(const eUpdatePriority priority, tUpdateFunc&& func, cUpdateSubscriberScope& scope) {
	const uint32_t bucketIdx = (uint32_t)priority;
	if (bucketIdx < mBuckets.size()) {
		sBucket& bucket = mBuckets[bucketIdx];
		if (nFlag::check(bucket.mFlags, sBucket::eInUse)) {
			mTempRecords.emplace_back(priority, std::move(func), scope);
		}
		else {
			bucket.mRecords.emplace_back(priority, std::move(func), scope);
			nFlag::set(bucket.mFlags, sBucket::eDirty);
		}
	}
}

void cUpdateQueue::begin_exec() {
	mExecutedPriority = eUpdatePriority::First;
}

namespace nEnum {
	template <typename TEnum> static inline TEnum add(const TEnum& e, int value) { return TEnum(typename std::underlying_type<TEnum>::type(e) + value); }
	template <typename TEnum> static inline TEnum incr(TEnum& e) { return (e = add(e, 1)); }
	template <typename TEnum> static inline TEnum decr(TEnum& e) { return (e = add(e, -1)); }
}

void cUpdateQueue::advance_exec(const eUpdatePriority targetPriority) {
	assert(mTempRecords.empty());

	const eUpdatePriority target = std::min(targetPriority, nEnum::add(eUpdatePriority::Count, -1));
	for (eUpdatePriority curPriority = nEnum::incr(mExecutedPriority); curPriority <= target; nEnum::incr(curPriority)) {
		const uint32_t bucketIdx = (uint32_t)curPriority;
		mBuckets[bucketIdx].exec();
		commit_temp_records();
	}
	mExecutedPriority = target;
}

void cUpdateQueue::end_exec() {
	mExecutedPriority = eUpdatePriority::Last;
}

void cUpdateQueue::commit_temp_records() {
	if (!mTempRecords.empty()) {
		for (cUpdateRecord& rec : mTempRecords) {
			if (rec.is_valid()) {
				const uint32_t bucketIdx = (uint32_t)rec.get_priority();
				mBuckets[bucketIdx].mRecords.emplace_back(std::move(rec));
				nFlag::set(mBuckets[bucketIdx].mFlags, sBucket::eDirty);
			}
		}
		mTempRecords.clear();
	}
}

/////////////////

cUpdateGraphNode::cUpdateGraphNode(NodeId id, const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope)
	: cUpdateRecord(eUpdatePriority::Begin, std::move(func), scope)
	, mDesc(desc)
	, mId(id)
{}

sUpdateDepRes cUpdateGraph::register_res(cstr name) {
	uint32_t hash = hash_fnv_1a_cstr(name);
	auto it = mResources.find(hash);
	if (mResources.end() == it) {
		it = mResources.emplace(hash, sResource{ hash, name }).first;
	}
	assert(it->first == it->second.hash);
	assert(name.p == it->second.name);
	return sUpdateDepRes(hash, it->second.name.c_str());
}

void cUpdateGraph::add(const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope) {
	NodeId id = mIdGen++;
	mPending.emplace_back(id, desc, std::move(func), scope);
}

void cUpdateGraph::exec_node(NodeId id) {
	auto it = mNodes.find(id);
	assert(it != mNodes.end());
	if (it != mNodes.end()) {
		if (!it->second.exec()) {
			mNodes.erase(it);
			mIsDirty = true;
		}
	}
}

struct cUpdateGraph::sTopoExecutor {
	cUpdateGraph& graph;
	sTopoExecutor(cUpdateGraph& graph) : graph(graph) {}

	void exec() {
		for (NodeId id : graph.mOrder) {
			graph.exec_node(id);
		}
	}
};

struct cUpdateGraph::sTaskExecutor {
	cUpdateGraph& graph;

	std::queue<NodeId> todo;

	struct sPrereq {
		uint32_t cur = 0;
		uint32_t target = 0;
	};
	using tPrereqMap = std::unordered_map<NodeId, sPrereq>;
	tPrereqMap prereqMap;

	sTaskExecutor(cUpdateGraph& graph) : graph(graph) {}
	void exec() {
		for (const auto& [id, node] : graph.mNodes) {
			auto inIt = graph.mRevAdjList.find(id);
			const uint32_t target = (inIt != graph.mRevAdjList.end()) ? (uint32_t)inIt->second.size() : 0;
			prereqMap[id] = sPrereq{ 0, target };
		}

		for (NodeId id : graph.mNoInputList) {
			todo.push(id);
		}

		// essntially, a Kahn's algorithm
		// https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm
		while (!todo.empty()) {
			NodeId cur = todo.front();
			todo.pop();
			graph.exec_node(cur);

			auto outIt = graph.mAdjList.find(cur);
			if (outIt != graph.mAdjList.end()) {
				for (NodeId next : outIt->second) {
					sPrereq& prereq = prereqMap[next];
					const uint32_t prereqCount = ++prereq.cur;
					if (prereqCount >= prereq.target) {
						assert(prereqCount == prereq.target);
						todo.push(next);
					}
				}
			}
		}
	}
};

void cUpdateGraph::exec() {
	commit_pending();
	update_dirty();

	//using tExecutor = sTopoExecutor;
	using tExecutor = sTaskExecutor;
	tExecutor(*this).exec();
}

void cUpdateGraph::commit_pending() {
	if (!mPending.empty()) {
		MICROPROFILE_SCOPEI("main", "cUpdateGraph::commit_pending", -1);
		for (cUpdateGraphNode& node : mPending) {
			mNodes.emplace(node.get_id(), std::move(node));
		}
		mPending.clear();
		mIsDirty = true;
	}
}

void cUpdateGraph::update_dirty() {
	if (mIsDirty) {
		MICROPROFILE_SCOPEI("main", "cUpdateGraph::update_dirty", -1);
		build_adj_list();
		topo_sort();
		mIsDirty = false;
	}
}

void cUpdateGraph::build_adj_list(const tEdgeMap& inEdgeMap, const tEdgeMap& outEdgeMap, tAdjList& adjList) {
	for (const auto& p : outEdgeMap) {
		auto it = inEdgeMap.find(p.first);
		if (it != inEdgeMap.end()) {
			for (NodeId outNode : p.second) {
				for (NodeId inNode : it->second) {
					if (outNode != inNode) {
						adjList[outNode].push_back(inNode);
					}
				}
			}
		}
	}
}

void cUpdateGraph::build_adj_list() {
	mAdjList.clear();
	mRevAdjList.clear();

	using tEdgeMap = std::unordered_map<uint32_t, std::vector<NodeId>>;
	tEdgeMap inEdgeMap;
	tEdgeMap outEdgeMap;
	for (const auto& idNode : mNodes) {
		const NodeId id = idNode.first;
		const auto& node = idNode.second;
		for (const sUpdateDepRes& dep : node.get_deps().inputs) {
			inEdgeMap[dep.hash].push_back(id);
		}
		if (node.get_deps().inputs.empty()) {
			mNoInputList.push_back(id);
		}

		for (const sUpdateDepRes& dep : node.get_deps().outputs) {
			outEdgeMap[dep.hash].push_back(id);
		}
	}

	build_adj_list(inEdgeMap, outEdgeMap, mAdjList);
	build_adj_list(outEdgeMap, inEdgeMap, mRevAdjList);
}

struct cUpdateGraph::sTopoSorter {
	enum class EMark { No, Temp, Perm };

	const cUpdateGraph& graph;
	std::unordered_map<NodeId, EMark> marks;
	tOrder order;

	sTopoSorter(cUpdateGraph& graph)
		: graph(graph)
	{}

	void sort() {
		for (const auto& idNode : graph.mNodes) {
			marks[idNode.first] = EMark::No;
		}

		for (const auto& idMark : marks) {
			if (idMark.second == EMark::No) {
				visit(idMark.first);
			}
		}

		std::reverse(order.begin(), order.end());
	}

private:
	void visit(NodeId id) {
		auto it = marks.find(id);
		assert(it != marks.end());

		if (it->second == EMark::Perm) { return; }
		if (it->second == EMark::Temp) {
			dbg_msg("Found cycle in node %u", id);
			return;
		}
		assert(it->second == EMark::No);

		it->second = EMark::Temp;
		auto adjIt = graph.mAdjList.find(id);
		if (adjIt != graph.mAdjList.end()) {
			for (NodeId adj : adjIt->second) {
				visit(adj);
			}
		}

		assert(it->second == EMark::Temp);
		it->second = EMark::Perm;

		order.push_back(id);
	}
};

void cUpdateGraph::topo_sort() {
	sTopoSorter sorter(*this);
	sorter.sort();
	mOrder = std::move(sorter.order);
}
