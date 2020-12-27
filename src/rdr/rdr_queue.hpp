#pragma once
#include <array>
#include <vector>


class cGfx;


struct iRdrJob {
	virtual ~iRdrJob() = default;
	virtual void disp_job(cRdrContext const& ctx) const = 0;
};


class cRdrQueueMgr {
	enum { CONTEXTS_COUNT = 4 };
	using JobsVector = std::vector<iRdrJob const*>;
	struct sJobsQueue {
		JobsVector prologueJobs;
		JobsVector jobs;
		bool bLocked = false;

		void clear();
	};

	std::array<com_ptr<ID3D11DeviceContext>, CONTEXTS_COUNT> mContexts;
	std::array<cConstBufStorage, CONTEXTS_COUNT> mBuffers;

	sJobsQueue mModelJobs;
public:
	static cRdrQueueMgr& get();

	cRdrQueueMgr(cGfx& gfx);

	void add_model_job(iRdrJob const& job);
	void add_model_prologue_job(iRdrJob const& job);


	void exec_model_jobs();

private:
	void exec_jobs_queue(sJobsQueue& queue);
	com_ptr<ID3D11CommandList> exec_jobs_batch(sJobsQueue const& queue, const size_t jobStartIdx, const size_t jobCount, const size_t batchIdx);
	void exec_prologue_jobs(cRdrContext const& ctx, JobsVector const& prologueJobs) const;

	void exec_jobs_queue_imm(sJobsQueue& queue);
};

