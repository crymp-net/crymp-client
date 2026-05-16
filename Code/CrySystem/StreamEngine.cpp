#include "CryCommon/CrySystem/CryFile.h"
#include "Library/WinAPI.h"

#include "CryLog.h"
#include "StreamEngine.h"

////////////////////////////////////////////////////////////////////////////////
// IReadStream
////////////////////////////////////////////////////////////////////////////////

void StreamEngine::Job::AddRef()
{
	this->refCount.fetch_add(1, std::memory_order_relaxed);
}

void StreamEngine::Job::Release()
{
	if (this->refCount.fetch_sub(1, std::memory_order_relaxed) <= 1)
	{
		delete this;
	}
}

bool StreamEngine::Job::IsError()
{
	return this->state.Get() == JobState::ERROR;
}

bool StreamEngine::Job::IsFinished()
{
	return this->state.Get() == JobState::CALLBACK_DONE;
}

unsigned int StreamEngine::Job::GetBytesRead(bool wait)
{
	if (wait)
	{
		this->Wait();
	}

	return this->bytesRead.load(std::memory_order_relaxed);
}

const void* StreamEngine::Job::GetBuffer()
{
	return this->userBuffer;
}

void StreamEngine::Job::Abort()
{
	this->state.Set(JobState::ABORTED);
}

void StreamEngine::Job::RaisePriority(unsigned int priority)
{
	CryLogErrorAlways("%s(%u): Not implemented!", __FUNCTION__, priority);
}

std::uintptr_t StreamEngine::Job::GetUserData()
{
	return this->userData;
}

void StreamEngine::Job::Wait()
{
	this->state.WaitUntilExecuted();

	if (this->engine->IsMainThread())
	{
		this->engine->DoCallback(*this);
	}
}

////////////////////////////////////////////////////////////////////////////////

StreamEngine::StreamEngine()
{
}

StreamEngine::~StreamEngine()
{
	m_submittedJobs.PushAndNotify({});
	m_workerThread.join();
}

////////////////////////////////////////////////////////////////////////////////
// IStreamEngine
////////////////////////////////////////////////////////////////////////////////

IReadStreamPtr StreamEngine::StartRead(const char* source, const char* filename, IStreamCallback* callback,
	StreamReadParams* params)
{
	_smart_ptr<Job> job(new Job);
	job->callback = callback;
	job->engine = this;

	if (source)
	{
		job->source = source;
	}

	if (filename)
	{
		job->filename = filename;
	}

	if (params)
	{
		job->userData = params->dwUserData;
		job->userBuffer = params->pBuffer;
		job->requestedOffset = params->nOffset;
		job->requestedSize = params->nSize;
		job->flags = params->nFlags;
	}

	m_submittedJobs.PushAndNotify(job);

	CryLogComment("StreamEngine(\"%s\", \"%s\", size=0x%x, offset=0x%x, flags=0x%x): Started",
		job->source.c_str(), job->filename.c_str(), job->requestedSize, job->requestedOffset, job->flags);


	return IReadStreamPtr(job.get());
}

unsigned int StreamEngine::GetFileSize(const char* filename, unsigned int cryPakFlags)
{
	CryFile file(filename, "rb");
	if (!file)
	{
		return 0;
	}

	const unsigned int size = static_cast<unsigned int>(file.GetSize());

	return size;
}

void StreamEngine::Update(unsigned int flags)
{
	_smart_ptr<Job> job;

	while (m_completedJobs.TryPop(job))
	{
		if (job)
		{
			this->DoCallback(*job);
		}
	}
}

unsigned int StreamEngine::Wait(unsigned int milliseconds, unsigned int flags)
{
	CryLogErrorAlways("%s(%u, 0x%x): Not implemented!", __FUNCTION__, milliseconds, flags);
	return 0;
}

void StreamEngine::GetMemoryStatistics(ICrySizer* sizer)
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

void StreamEngine::SuspendCallbackTimeQuota()
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

void StreamEngine::ResumeCallbackTimeQuota()
{
	CryLogErrorAlways("%s: Not implemented!", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////////

bool StreamEngine::IsMainThread()
{
	return std::this_thread::get_id() == m_mainThreadID;
}

void StreamEngine::WorkerThreadLoop()
{
	WinAPI::SetCurrentThreadName("Streaming");

	_smart_ptr<Job> job;

	while (true)
	{
		m_submittedJobs.BlockingPop(job);

		if (!job)
		{
			// joining now
			break;
		}

		this->ExecuteJob(*job);

		m_completedJobs.Push(job);
	}
}

void StreamEngine::ExecuteJob(Job& job)
{
	if (job.state.Get() != JobState::PENDING)
	{
		return;
	}

	CryFile file(job.filename.c_str(), "rb");
	if (!file)
	{
		job.state.Set(JobState::ERROR);
		return;
	}

	if (job.requestedOffset > 0)
	{
		file.Seek(job.requestedOffset, SEEK_SET);
	}

	unsigned int size = job.requestedSize;
	if (!size)
	{
		size = static_cast<unsigned int>(file.GetSize());

		if (job.requestedOffset < size)
		{
			size -= job.requestedOffset;
		}
		else
		{
			size = 0;
		}
	}

	if (!job.userBuffer)
	{
		job.buffer = std::make_unique<unsigned char[]>(size);
		job.userBuffer = job.buffer.get();
	}

	const unsigned int bytesRead = static_cast<unsigned int>(file.ReadRaw(job.userBuffer, size));

	job.bytesRead.store(bytesRead, std::memory_order_relaxed);
	job.state.Set(JobState::FINISHED);
}

void StreamEngine::DoCallback(Job& job)
{
	unsigned int error = 0;

	switch (job.state.Get())
	{
		case JobState::ERROR:
		{
			error = ERROR_CANT_OPEN_FILE;
			break;
		}
		case JobState::ABORTED:
		{
			error = ERROR_USER_ABORT;
			break;
		}
		case JobState::FINISHED:
		{
			break;
		}
		case JobState::PENDING:
		case JobState::CALLBACK_DONE:
		{
			return;
		}
	}

	if (job.callback)
	{
		job.callback->StreamOnComplete(&job, error);
	}

	job.state.Set(JobState::CALLBACK_DONE);

	CryLogComment("StreamEngine(\"%s\", \"%s\", size=0x%x, offset=0x%x, flags=0x%x): Finished",
		job.source.c_str(), job.filename.c_str(), job.requestedSize, job.requestedOffset, job.flags);
}
