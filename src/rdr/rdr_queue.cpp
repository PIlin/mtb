#include "common.hpp"
#include "math.hpp"
#include "gfx.hpp"
#include "rdr.hpp"
#include "rdr_queue.hpp"


void cRdrQueueMgr::sJobsQueue::clear() {
	jobs.clear();
	prologueJobs.clear();
}


cRdrQueueMgr::cRdrQueueMgr(cGfx& gfx) {
	auto* pDev = gfx.get_dev();

	for (auto& pCtx : mContexts) {
		HRESULT hr = pDev->CreateDeferredContext(0, pCtx.pp());
		if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateDeferredContext failed");
	}

	for (auto& storage : mBuffers) {
		storage.init(pDev);
	}
}

void cRdrQueueMgr::add_model_job(iRdrJob const& job) {
	assert(!mModelJobs.bLocked);
	mModelJobs.jobs.push_back(&job);
}

void cRdrQueueMgr::add_model_prologue_job(iRdrJob const& job) {
	assert(!mModelJobs.bLocked);
	mModelJobs.prologueJobs.push_back(&job);
}

void cRdrQueueMgr::exec_model_jobs() {
	exec_jobs_queue(mModelJobs);
	//exec_jobs_queue_imm(mModelJobs);
}

void cRdrQueueMgr::exec_jobs_queue(sJobsQueue& queue) {
	std::array<com_ptr<ID3D11CommandList>, CONTEXTS_COUNT> commandLists;
	size_t commandListsCount = 0;

	{
		auto jobsLock = sScopedExecutor::create(
			[&] { assert(!queue.bLocked);  queue.bLocked = true; },
			[&] { assert(queue.bLocked); queue.bLocked = false; });

		const size_t count = queue.jobs.size();
		// simulate several threads
		const size_t batchSize = count / CONTEXTS_COUNT;
		size_t startIdx = 0;
		if (batchSize > 0) {
			for (size_t i = 0; i < CONTEXTS_COUNT - 1; ++i) {
				commandLists[commandListsCount++] = exec_jobs_batch(queue, startIdx, batchSize, i);
				startIdx += batchSize;
			}
		}

		commandLists[commandListsCount++] = exec_jobs_batch(queue, startIdx, count - startIdx, CONTEXTS_COUNT - 1);
		queue.clear();
	}

	auto pImmCtx = get_gfx().get_imm_ctx();
	for (size_t i = 0; i < commandListsCount; ++i) {
		auto& pCL = commandLists[i];
		pImmCtx->ExecuteCommandList(pCL, FALSE);
	}
}

com_ptr<ID3D11CommandList> cRdrQueueMgr::exec_jobs_batch(sJobsQueue const& queue, const size_t jobStartIdx, const size_t jobCount, const size_t batchIdx) {
	cRdrContext ctx(mContexts[batchIdx], mBuffers[batchIdx]);
	exec_prologue_jobs(ctx, queue.prologueJobs);
	auto const& jobs = queue.jobs;
	for (size_t idx = jobStartIdx, endIdx = jobStartIdx + jobCount; idx < endIdx; ++idx) {
		jobs[idx]->disp_job(ctx);
	}

	com_ptr<ID3D11CommandList> pCL;
	ctx.get_ctx()->FinishCommandList(FALSE, pCL.pp());
	return pCL;
}

void cRdrQueueMgr::exec_prologue_jobs(cRdrContext const& ctx, JobsVector const& prologueJobs) const {
	for (auto* pJob : prologueJobs) {
		pJob->disp_job(ctx);
	}
}

void cRdrQueueMgr::exec_jobs_queue_imm(sJobsQueue& queue) {
	auto jobsLock = sScopedExecutor::create(
		[&] { assert(!queue.bLocked);  queue.bLocked = true; },
		[&] { assert(queue.bLocked); queue.bLocked = false; });

	auto ctx = cRdrContext::create_imm();
	
	for (auto* pJob : queue.prologueJobs) {
		pJob->disp_job(ctx);
	}
	for (auto* pJob : queue.jobs) {
		pJob->disp_job(ctx);
	}

	queue.clear();
}


