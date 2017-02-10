#include <array>
#include <functional>
#include <vector>

class cUpdateRecord;

class cUpdateSubscriberScope : noncopyable {
	cUpdateRecord* mpUpdateRecord = nullptr;
public:

	cUpdateSubscriberScope() = default;
	~cUpdateSubscriberScope() { reset(); }
	void reset();
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
	ScenePreDisp,
	SceneDisp,
	End,
	Count,
	Last = Count
};

using tUpdateFunc = std::function<void()>;

class cUpdateRecord : noncopyable {
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
