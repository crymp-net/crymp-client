#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "CryCommon/CrySystem/IStreamEngine.h"

class StreamEngine final : public IStreamEngine
{
	enum class JobState
	{
		Pending, Error, Aborted, Finished, CallbackDone
	};

	class WaitableJobState
	{
		std::atomic<JobState> state{JobState::Pending};
		std::mutex mutex;
		std::condition_variable cv;

	public:
		JobState Get()
		{
			return this->state.load(std::memory_order_relaxed);
		}

		void Set(JobState state)
		{
			std::lock_guard lock(this->mutex);
			this->state.store(state, std::memory_order_relaxed);
			this->cv.notify_all();
		}

		void WaitUntilExecuted()
		{
			std::unique_lock lock(this->mutex);
			this->cv.wait(lock, [this]() { return this->Get() != JobState::Pending; });
		}
	};

	struct Job final : public IReadStream
	{
		std::atomic<int> refCount = 0;
		WaitableJobState state;
		unsigned int flags = 0;
		unsigned int requestedSize = 0;
		unsigned int requestedOffset = 0;
		std::atomic<unsigned int> bytesRead = 0;
		std::uintptr_t userData = 0;
		void* userBuffer = nullptr;
		IStreamCallback* callback = nullptr;
		std::string source;
		std::string filename;
		std::unique_ptr<unsigned char[]> buffer;
		StreamEngine* engine = nullptr;

		////////////////////////////////////////////////////////////////////////////////
		// IReadStream
		////////////////////////////////////////////////////////////////////////////////

		void AddRef() override;
		void Release() override;

		bool IsError() override;
		bool IsFinished() override;

		unsigned int GetBytesRead(bool wait = false) override;
		const void* GetBuffer() override;

		void Abort() override;

		void RaisePriority(unsigned int priority) override;

		std::uintptr_t GetUserData() override;

		void Wait() override;

		////////////////////////////////////////////////////////////////////////////////
	};

	template<class T>
	class CompletionQueue
	{
	protected:
		std::mutex mutex;
		std::deque<T> queue;

	public:
		void Push(const T& element)
		{
			std::lock_guard lock(this->mutex);
			this->queue.push_back(element);
		}

		bool TryPop(T& element)
		{
			std::lock_guard lock(this->mutex);

			if (this->queue.empty())
			{
				return false;
			}

			element = this->queue.front();
			this->queue.pop_front();
			return true;
		}
	};

	template<class T>
	class SubmissionQueue : private CompletionQueue<T>
	{
		std::condition_variable cv;

	public:
		void PushAndNotify(const T& element)
		{
			this->Push(element);
			this->cv.notify_one();
		}

		void BlockingPop(T& element)
		{
			std::unique_lock lock(this->mutex);
			this->cv.wait(lock, [this]() { return !this->queue.empty(); });
			element = this->queue.front();
			this->queue.pop_front();
		}
	};

	std::thread m_workerThread{[this]() { this->WorkerThreadLoop(); }};
	std::thread::id m_mainThreadID{std::this_thread::get_id()};
	SubmissionQueue<_smart_ptr<Job>> m_submittedJobs;
	CompletionQueue<_smart_ptr<Job>> m_completedJobs;

public:
	// to be removed once we have our own CrySystem
	static StreamEngine& GetInstance()
	{
		static StreamEngine* instance = new StreamEngine;
		return *instance;
	}

	StreamEngine();
	~StreamEngine();

	////////////////////////////////////////////////////////////////////////////////
	// IStreamEngine
	////////////////////////////////////////////////////////////////////////////////

	IReadStreamPtr StartRead(const char* source, const char* filename, IStreamCallback* callback = nullptr,
		StreamReadParams* params = nullptr) override;

	unsigned int GetFileSize(const char* filename, unsigned int cryPakFlags = 0) override;

	void Update(unsigned int flags = 0) override;

	unsigned int Wait(unsigned int milliseconds, unsigned int flags = 0) override;

	void GetMemoryStatistics(ICrySizer* sizer) override;

	void SuspendCallbackTimeQuota() override;
	void ResumeCallbackTimeQuota() override;

	////////////////////////////////////////////////////////////////////////////////

private:
	bool IsMainThread();

	void WorkerThreadLoop();
	void ExecuteJob(Job& job);
	void DoCallback(Job& job);
};
