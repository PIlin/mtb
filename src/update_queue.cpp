#include "common.hpp"
#include "update_queue.hpp"

#include <cassert>


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

