#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// CryEngine 2 Win32 compatibility API expected by Wars CrySystem.
using THREAD_HANDLE = void*;
using EVENT_HANDLE = void*;

inline void CrySleep(unsigned int milliseconds)
{
    ::Sleep(milliseconds);
}

inline void* CryCreateCriticalSection()
{
    auto* cs = new CRITICAL_SECTION;
    ::InitializeCriticalSection(cs);
    return cs;
}
inline void CryDeleteCriticalSection(void* cs)
{
    if (!cs) return;
    ::DeleteCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
    delete static_cast<CRITICAL_SECTION*>(cs);
}
inline void CryEnterCriticalSection(void* cs) { ::EnterCriticalSection(static_cast<CRITICAL_SECTION*>(cs)); }
inline bool CryTryCriticalSection(void* cs) { return ::TryEnterCriticalSection(static_cast<CRITICAL_SECTION*>(cs)) != FALSE; }
inline void CryLeaveCriticalSection(void* cs) { ::LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(cs)); }

#ifndef CRYLOCK_FAST
#define CRYLOCK_FAST 0
#endif

struct CryRunnable
{
    virtual ~CryRunnable() = default;
    virtual void Run() = 0;
    virtual void Cancel() {}
};

template<int LockType = CRYLOCK_FAST>
class CryCondLock
{
public:
    void Lock() { m_mutex.lock(); }
    void Unlock() { m_mutex.unlock(); }
private:
    std::recursive_mutex m_mutex;
    template<class> friend class CryCond;
};

using CryFastLock = CryCondLock<CRYLOCK_FAST>;

template<class Lock>
class CryCond
{
public:
    void Wait(Lock& lock)
    {
        std::unique_lock<std::recursive_mutex> guard(lock.m_mutex, std::adopt_lock);
        m_cv.wait(guard);
        guard.release(); // caller still owns the lock after Wait(), matching CE2 semantics
    }
    void Notify() { m_cv.notify_all(); }
    void NotifySingle() { m_cv.notify_one(); }
private:
    std::condition_variable_any m_cv;
};

namespace CryThreadCompat
{
    inline std::mutex g_namesMutex;
    inline std::unordered_map<DWORD, std::string> g_names;
}

inline void CryThreadSetName(DWORD threadId, const char* name)
{
    if (threadId == DWORD(-1) || threadId == 0)
        threadId = ::GetCurrentThreadId();
    {
        std::lock_guard<std::mutex> lock(CryThreadCompat::g_namesMutex);
        CryThreadCompat::g_names[threadId] = name ? name : "";
    }
#if defined(_WIN32)
    if (threadId == ::GetCurrentThreadId() && name)
    {
        using SetThreadDescriptionFn = HRESULT (WINAPI*)(HANDLE, PCWSTR);
        static auto fn = reinterpret_cast<SetThreadDescriptionFn>(
            ::GetProcAddress(::GetModuleHandleW(L"Kernel32.dll"), "SetThreadDescription"));
        if (fn)
        {
            wchar_t wide[128]{};
            ::MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, 128);
            fn(::GetCurrentThread(), wide);
        }
    }
#endif
}

inline const char* CryThreadGetName(DWORD threadId)
{
    if (threadId == DWORD(-1) || threadId == 0)
        threadId = ::GetCurrentThreadId();
    thread_local std::string result;
    std::lock_guard<std::mutex> lock(CryThreadCompat::g_namesMutex);
    auto it = CryThreadCompat::g_names.find(threadId);
    result = it != CryThreadCompat::g_names.end() ? it->second : std::string();
    return result.c_str();
}

template<class RunnableT = CryRunnable>
class CryThread
{
public:
    CryThread() = default;
    virtual ~CryThread() { Stop(); }
    CryThread(const CryThread&) = delete;
    CryThread& operator=(const CryThread&) = delete;

    virtual void Run() = 0;
    virtual void Cancel() {}

    virtual void Start(unsigned /*cpuMask*/ = 0)
    {
        StartImpl([this] { this->Run(); });
    }

    virtual void Start(RunnableT& runnable, unsigned /*cpuMask*/ = 0)
    {
        StartImpl([&runnable] { runnable.Run(); });
    }

    void Stop()
    {
        NotifySingle();
        if (m_thread.joinable())
        {
            if (m_thread.get_id() == std::this_thread::get_id())
                m_thread.detach();
            else
                m_thread.join();
        }
        m_started.store(false, std::memory_order_release);
    }

    bool IsStarted() const { return m_started.load(std::memory_order_acquire); }
    bool IsRunning() const { return IsStarted(); }

protected:
    void Lock() { m_waitMutex.lock(); }
    void Unlock() { m_waitMutex.unlock(); }
    void Wait()
    {
        std::unique_lock<std::recursive_mutex> guard(m_waitMutex, std::adopt_lock);
        m_waitCv.wait(guard);
        guard.release();
    }
    void NotifySingle() { m_waitCv.notify_one(); }
    void Notify() { m_waitCv.notify_all(); }

private:
    template<class Fn>
    void StartImpl(Fn&& fn)
    {
        if (m_thread.joinable())
            return;
        m_started.store(true, std::memory_order_release);
        m_thread = std::thread([this, task = std::forward<Fn>(fn)]() mutable {
            task();
            m_started.store(false, std::memory_order_release);
        });
    }

    std::thread m_thread;
    std::atomic_bool m_started{false};
    std::recursive_mutex m_waitMutex;
    std::condition_variable_any m_waitCv;
};

template<class RunnableT = CryRunnable>
class CrySimpleThread : public CryThread<RunnableT>
{
public:
    using CryThread<RunnableT>::CryThread;
    static CrySimpleThread* Self() { return nullptr; }
    void SetName(const char* name) { CryThreadSetName(::GetCurrentThreadId(), name); }
};
