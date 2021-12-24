#include "common.hpp"
#include "update_queue.hpp"

#include "dbg_ui.hpp"
#include "imgui.h"

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

bool cUpdateRecord::exec() const {
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

sUpdateDepRes cUpdateGraph::register_res(cstr nameGroup, cstr nameType) {
	if (nameType) {
		register_res_impl(nameGroup, nullptr);
	}
	return register_res_impl(nameGroup, nameType);
}

sUpdateDepRes cUpdateGraph::register_res_impl(cstr nameGroup, cstr nameType) {
	uint32_t hashGroup = hash_fnv_1a_cstr(nameGroup);
	uint32_t hashType = nameType ? hash_fnv_1a_cstr(nameType) : 0;
	assert(!nameType || hashType != 0);

	sResourceHash h{ hashGroup, hashType };

	auto it = mResources.find(h);
	if (mResources.end() == it) {
		it = mResources.emplace(h, sResource{ h, nameGroup, nameType ? nameType : cstr("") }).first;
	}
	assert(it->first == it->second.hash);
	assert(nameGroup.p == it->second.nameGroup);
	assert(!nameType || nameType.p == it->second.nameType);
	return sUpdateDepRes(hashGroup, it->second.nameGroup.c_str(), hashType, it->second.nameType.c_str());
}

void cUpdateGraph::add(const sUpdateDepDesc& desc, tUpdateFunc&& func, cUpdateSubscriberScope& scope) {
	NodeId id = mIdGen++;
	mPending.emplace_back(id, desc, std::move(func), scope);
}

void cUpdateGraph::exec_node(NodeId id) {
	auto it = mNodes.find(id);
	assert(it != mNodes.end());
	if (it != mNodes.end()) {
		cUpdateGraphNode& node = it->second;
		if (!dbg_on_exec_node(node))
			return;
		if (!node.exec()) {
			mNodes.erase(it);
			mark_dirty();
		}
	}
}

bool cUpdateGraph::dbg_on_exec_node(cUpdateGraphNode const& node) {
	bool isEnabled = true;
	if (cDbgToolsMgr::tools().update_queue) {
		if (ImGui::Begin("update graph")) {
			ImGui::PushID(node.get_id());
			if (ImGui::BeginTable("updatenode", 3, ImGuiTableFlags_Borders)) {
				ImGui::TableNextColumn();

				auto stateIter = mDbgNodeState.insert(std::make_pair(node.get_id(), true)).first;
				ImGui::Checkbox("##enabled", &stateIter->second); ImGui::SameLine();
				isEnabled = stateIter->second;

				ImGui::Text("%u %s", node.get_id(), node.get_dbg_name());
				ImGui::TableNextColumn();

				auto renderTable = [&](const char* tableName, const std::vector<sUpdateDepRes>& deps) {
					ImGuiSelectableFlags flags = 0;
					ImVec2 alignment = { 1.0, 0.5 };

					if (ImGui::BeginTable(tableName, 2)) {
						for (sUpdateDepRes const& d : deps) {
							bool& state = mDbgResourceHighlight[d];
							bool& stateGroup = mDbgResourceHighlight[d.get_group()];

							ImGui::PushID(d.hashGroup);
							ImGui::PushID(d.hashType);

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, alignment);
							ImGui::Selectable(d.nameGroup, &stateGroup, flags);
							ImGui::PopStyleVar();

							if (d.nameType) {
								ImGui::TableSetColumnIndex(1);
								ImGui::Selectable(d.nameType, &state, flags);
							}

							ImGui::PopID();
							ImGui::PopID();
						}
						ImGui::EndTable();
					}
				};

				renderTable("tableInputs", node.get_deps().inputs);

				ImGui::TableNextColumn();
				renderTable("tableOutputs", node.get_deps().outputs);

				ImGui::EndTable();
			}
			ImGui::PopID();
		}
		ImGui::End();
	}
	return isEnabled;
}

struct cUpdateGraph::sTopoExecutor {
	cUpdateGraph& graph;
	sTopoExecutor(cUpdateGraph& graph) : graph(graph) {}

	void exec() {
		graph.topo_sort();
		assert(!graph.mIsOrderDirty);

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

		assert(graph.mNoInputList.size() <= graph.mNodes.size());
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

	if (bool& isOpen = cDbgToolsMgr::tools().update_queue) {
		if (ImGui::Begin("update graph", &isOpen)) {
			ImGui::Checkbox("Use topo executor", &mDbgUseTopoExecutor);
		}
		ImGui::End();
	}

	if (mDbgUseTopoExecutor)
		sTopoExecutor(*this).exec();
	else
		sTaskExecutor(*this).exec();
}

void cUpdateGraph::commit_pending() {
	if (!mPending.empty()) {
		MICROPROFILE_SCOPEI("main", "cUpdateGraph::commit_pending", -1);
		for (cUpdateGraphNode& node : mPending) {
			mNodes.emplace(node.get_id(), std::move(node));
		}
		mPending.clear();
		mIsDirty = true;
		mark_dirty();
	}
}

void cUpdateGraph::update_dirty() {
	if (mIsDirty) {
		MICROPROFILE_SCOPEI("main", "cUpdateGraph::update_dirty", -1);
		build_adj_list();
		mIsDirty = false;
	}
}

void cUpdateGraph::mark_dirty() {
	mIsDirty = true;
	mIsOrderDirty = true;
}


void cUpdateGraph::build_adj_list(const tEdgeMap& inEdgeMap, const tEdgeMap& outEdgeMap, tAdjList& adjList) {
	auto addAdjacency = [&adjList](const std::vector<NodeId>& out, const std::vector<NodeId>& in) {
		for (NodeId outNode : out) {
			for (NodeId inNode : in) {
				if (outNode != inNode) {
					adjList[outNode].push_back(inNode);
				}
			}
		}
	};

	for (const auto& pOut : outEdgeMap) {
		const sResourceHash& outRh = pOut.first;
		if (outRh.type != 0) {
			auto it = inEdgeMap.find(outRh);
			if (it != inEdgeMap.end()) {
				addAdjacency(pOut.second, it->second);
			}
			it = inEdgeMap.find(outRh.get_group());
			if (it != inEdgeMap.end()) {
				addAdjacency(pOut.second, it->second);
			}
		}
		else {
			// output is group - connect to all inputs of same group
			for (const auto& pIn : inEdgeMap) {
				if (pIn.first.group == outRh.group) {
					addAdjacency(pOut.second, pIn.second);
				}
			}
		}
	}

	for (auto& p : adjList) {
		std::vector<NodeId>& v = p.second;
		std::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
	}
}

void cUpdateGraph::build_adj_list() {
	mAdjList.clear();
	mRevAdjList.clear();
	mNoInputList.clear();

	tEdgeMap inEdgeMap;
	tEdgeMap outEdgeMap;
	for (const auto& idNode : mNodes) {
		const NodeId id = idNode.first;
		const auto& node = idNode.second;
		for (const sUpdateDepRes& dep : node.get_deps().inputs) {
			inEdgeMap[dep].push_back(id);
		}
		if (node.get_deps().inputs.empty()) {
			mNoInputList.push_back(id);
		}

		for (const sUpdateDepRes& dep : node.get_deps().outputs) {
			outEdgeMap[dep].push_back(id);
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
	if (mIsOrderDirty) {
		MICROPROFILE_SCOPEI("main", "cUpdateGraph::topo_sort", -1);

		sTopoSorter sorter(*this);
		sorter.sort();
		mOrder = std::move(sorter.order);
		mIsOrderDirty = false;
	}
}
