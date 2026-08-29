
//#include <vcl>
#include <windows.h>
#include "ThreadSampler.h"
#include <intrin.h>
#include <assert.h>
#include <tlhelp32.h>
#include <mmsystem.h>

const int FIRST_IOCTL_INDEX = 0x800;
const int FILE_DEVICE_myDrv = 0x00008000;

#ifndef METHOD_BUFFERED
const int METHOD_BUFFERED   = 0;
#endif

#ifndef FILE_ANY_ACCESS
const int FILE_ANY_ACCESS   = 0;
#endif

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Func, Method, Access )  \
  ((DeviceType) << 16) | ((Access) << 14) | ((Func) << 2) | (Method)
#endif

//===============================
//THREADNAME_INFO
//===============================
typedef struct
{
   DWORD dwType; // must be 0x1000
   LPCSTR szName; // pointer to name (in user addr space)
   DWORD dwThreadID; // thread ID (-1=caller thread)
   DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

TThreadSampler::TThreadNames TThreadSampler::threadNames;

//============================================================
//TThreadSampler::TThreadSampler
//============================================================
TThreadSampler::TThreadSampler()
{
 lastContextSwitches = 0;
 lastRDTSC = RDTSC();
 lastSnapshot = GetTickCount()-SNAPSHOT_UPDATE_PERIOD*2;
 RDTSCperSecond = 0;
 QueryPerformanceCounter((LARGE_INTEGER*)&lastPC);
 LoadDriver();
 SetThreadName("Main_Thread");
#ifdef TIMERSAMPLER
 timeBeginPeriod(1);
#endif
}

//============================================================
//TThreadSampler::~TThreadSampler
//============================================================
TThreadSampler::~TThreadSampler()
{
 UnloadDriver();

 for (TThreadNames::iterator iter = threadNames.begin(); iter!=threadNames.end(); ++iter)
  {
   free(iter->second);
  }
}


//============================================================
//void TThreadSampler::LoadDriver()
//============================================================
void TThreadSampler::LoadDriver()
{
 SC_HANDLE sch;
 SC_HANDLE schService;

 sch = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

 if (sch == 0) return;

#ifdef TIMERSAMPLER
 schService = OpenService( sch, "myThreadMon1", SERVICE_ALL_ACCESS);
#else
 schService = OpenService( sch, "myThreadMon", SERVICE_ALL_ACCESS);
#endif

 if (schService==0)
  {
   char fname[MAX_PATH];
   GetDriverFileName(fname);

#ifdef TIMERSAMPLER
   schService=CreateService(sch,"myThreadMon1","myThreadMon1",SERVICE_ALL_ACCESS,
                            SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
                            fname,NULL,NULL,NULL,NULL,NULL);
#else
   schService=CreateService(sch,"myThreadMon","myThreadMon",SERVICE_ALL_ACCESS,
                            SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
                            fname,NULL,NULL,NULL,NULL,NULL);
#endif
  }

 if(schService!=0)
  {
   StartService(schService,0,NULL);
   CloseServiceHandle(schService);
  }

 CloseServiceHandle(sch);
}

//============================================================
//void TThreadSampler::UnloadDriver()
//============================================================
void TThreadSampler::UnloadDriver()
{
 SC_HANDLE sch;
 SC_HANDLE schService;

 sch = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

 if (sch == 0) return;

#ifdef TIMERSAMPLER
 schService = OpenService( sch, "myThreadMon1", SERVICE_ALL_ACCESS);
#else
 schService = OpenService( sch, "myThreadMon", SERVICE_ALL_ACCESS);
#endif

 if (schService!=0)
  {
   SERVICE_STATUS serviceStatus;
   ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus);
   DeleteService(schService);
   CloseServiceHandle(schService);
  }

 CloseServiceHandle(sch);
}

//============================================================
//============================================================
bool FileExists(LPCTSTR fname)
{
 return GetFileAttributes(fname) != DWORD(-1);
}

//============================================================
//void TThreadSampler::GetDriverFileName()
//============================================================
void TThreadSampler::GetDriverFileName(char* fname)
{
 char* dummy;
 GetModuleFileName(NULL,fname,MAX_PATH);
 fname[MAX_PATH-1]=0;

 GetFullPathName(fname,MAX_PATH,fname,&dummy);
 assert(dummy!=NULL);

 dummy[0]='\0';

#ifdef TIMERSAMPLER
 assert(strlen("irq8_hook.sys")+strlen(fname)<MAX_PATH-1);
 strcat(fname,"irq8_hook.sys");
#else
 assert(strlen("swap_hook.sys")+strlen(fname)<MAX_PATH-1);
 strcat(fname,"swap_hook.sys");
#endif

 //if driver file is not present in executable directory, search in c:\windows\system32\

 if (FileExists(fname)==false)
  {
   GetWindowsDirectory(fname, MAX_PATH);
#ifdef TIMERSAMPLER
   assert(strlen("system32\\irq8_hook.sys")+strlen(fname)<MAX_PATH-1);
   strcat(fname,"system32\\irq8_hook.sys");
#else
   assert(strlen("system32\\swap_hook.sys")+strlen(fname)<MAX_PATH-1);
   strcat(fname,"system32\\swap_hook.sys");
#endif
  }
}


//============================================================
//bool TThreadSampler::MakeSnapshoot()
//============================================================
bool TThreadSampler::MakeSnapshot(DWORD* contextSwitches)
{
 *contextSwitches = 0;

 //request driver data only once per two seconds
 if (GetTickCount()-lastSnapshot<SNAPSHOT_UPDATE_PERIOD)
  {
   *contextSwitches = lastContextSwitches;
   return true;
  }
 lastSnapshot = GetTickCount();

 //------ calculate average RDTSCperSecond  -----
 __int64 thisRDTSC = RDTSC();
 __int64 thisPC, freq;
 QueryPerformanceCounter((LARGE_INTEGER*)&thisPC);
 QueryPerformanceFrequency((LARGE_INTEGER*)&freq);

 __int64 rdiff = thisRDTSC-lastRDTSC;
 lastRDTSC=thisRDTSC;
 __int64 cdiff = thisPC-lastPC;
 lastPC=thisPC;

 //prevent overflow
 rdiff/=1024;
 cdiff/=1024;
 RDTSCperSecond = rdiff*freq/cdiff;

 //----------------------------------------------

 //request snapshot from driver

#ifdef TIMERSAMPLER

 TSamplesCPU CPUDriverData[MAX_CPU_COUNT];

 HANDLE hDevice = CreateFile("//./myThreadMon1", GENERIC_READ,
     FILE_SHARE_READ | FILE_SHARE_WRITE,
     NULL, OPEN_EXISTING,
     FILE_ATTRIBUTE_NORMAL,  0);

 if (hDevice == INVALID_HANDLE_VALUE) return false;

 DWORD retCount;

 DeviceIoControl(hDevice,
             CTL_CODE(FILE_DEVICE_myDrv, 0x800+101, METHOD_BUFFERED, FILE_ANY_ACCESS),
             NULL, 0, &CPUDriverData, sizeof(TSamplesCPU)*MAX_CPU_COUNT, &retCount, 0);


 DWORD ll = sizeof(TSamplesCPU);

 assert(retCount==sizeof(TSamplesCPU)*MAX_CPU_COUNT);

 MergeCPUData(&CPUDriverData[0]);

 *contextSwitches = 0;

#else

 HANDLE hDevice = CreateFile("//./myThreadMon0", GENERIC_READ,
     FILE_SHARE_READ | FILE_SHARE_WRITE,
     NULL, OPEN_EXISTING,
     FILE_ATTRIBUTE_NORMAL,  0);

 if (hDevice == INVALID_HANDLE_VALUE) return false;

 DWORD retCount;

 DeviceIoControl(hDevice,
             CTL_CODE(FILE_DEVICE_myDrv, 0x800+101, METHOD_BUFFERED, FILE_ANY_ACCESS),
             NULL, 0, &samples.driverData, sizeof(samples.driverData), &retCount, 0);

 assert(retCount==sizeof(samples.driverData));

 __int64 csw = samples[samples.size()-1].RDTSC-samples[0].RDTSC;
 csw/=16384;
 if (csw<1) csw = 1;
 __int64 rt = RDTSCperSecond/16384;
 csw=samples.size()*rt/csw;
 *contextSwitches = (DWORD)csw;

#endif

 lastContextSwitches = *contextSwitches;

 CloseHandle(hDevice);

 return true;
}

//============================================================
//void TThreadSampler::SetThreadName()
//============================================================
void TThreadSampler::SetThreadName(const char* name)
{
 //SetThreadNameEx(GetCurrentThreadId(), name);
}

//============================================================
//void TThreadSampler::SetThreadNameEx()
//============================================================
void TThreadSampler::SetThreadNameEx(DWORD threadId, const char* name)
{
	/*
 //set thread name using undocumented exception
 THREADNAME_INFO info;
 info.dwType = 0x1000;
 info.szName = name;
 info.dwThreadID = threadId;
 info.dwFlags = 0;

 __try
 {
  RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info );
 }
 __except(EXCEPTION_CONTINUE_EXECUTION)
 {
 }

 //keep in map
 TThreadNames::iterator iter = threadNames.find(threadId);
 if (iter==threadNames.end())
  {
   threadNames.insert(std::make_pair(threadId, strdup(name)));
  }
   else
  {
   free(iter->second);
   iter->second = strdup(name);
  }
	*/
}

//========================================================================
__int64 TThreadSampler::RDTSC()
{
    const DWORD_PTR previousMask = SetThreadAffinityMask(GetCurrentThread(), 1);
    ::Sleep(0);
    const unsigned __int64 cycles = __rdtsc();
    if (previousMask)
        SetThreadAffinityMask(GetCurrentThread(), previousMask);
    return static_cast<__int64>(cycles);
}

#ifdef TIMERSAMPLER
//============================================================
//void TThreadSampler::CreateSpanListForThread()
//============================================================
void TThreadSampler::CreateSpanListForThread(DWORD processId, DWORD threadId, TSpanList& spanList, DWORD width, DWORD scale, DWORD* totalTime)
{
 if (RDTSCperSecond==0) return;

 __int64 count = 0;

 //show lastest info
 //find starting sample index

 DWORD startIndex = samples.size()-1;
 __int64 period = RDTSCperSecond*scale/2;

 while (startIndex>0)
  {
   if (samples[samples.size()-1].RDTSC-samples[startIndex].RDTSC>period) break;
   startIndex--;
  }

 DWORD i = startIndex;
 while (i<samples.size()-1)
  {
   if ((threadId==OTHER_THREADS && samples[i].processId != processId && samples[i].processId!=0) ||
       (samples[i].processId == processId && samples[i].threadId == threadId))
        {
									//assume i+1 sample is an end of execution of current thread

									__int64 start = samples[i].RDTSC-samples[startIndex].RDTSC;
									__int64 end = samples[i+1].RDTSC-samples[startIndex].RDTSC;

         if (end<start) end=start;

         if (start>RDTSCperSecond*scale/2)
          {
           break;
          }

         if (end>RDTSCperSecond*scale/2)
          {
           end = RDTSCperSecond*scale/2;
          }

         count+=end-start;

									start = start*width*2/RDTSCperSecond/scale;
									end = end*width*2/RDTSCperSecond/scale;

									if (start>=width) break;

									if (end==start) end++;

									if (end>width-1) end=width-1;

									//try to merge with previous span
									if (spanList.size()>0)
									 {
									  if (spanList[spanList.size()-1].end >= (WORD)start)
									   {
									    spanList[spanList.size()-1].end = (WORD)end;
									   }
									    else
									   {
									    TSpan span;
									    span.start = (WORD)start;
									    span.end = (WORD)end;
									    spanList.push_back(span);
									   }
									 }
									  else
									 {
									  TSpan span;
									  span.start = (WORD)start;
									  span.end = (WORD)end;
									  spanList.push_back(span);
									 }
        }
   i++;
  }

  __int64 s = RDTSCperSecond*scale/2;
 *totalTime = (DWORD)((count*1000+s/2)/s);
}

#else

//============================================================
//void TThreadSampler::CreateSpanListForThread()
//============================================================
void TThreadSampler::CreateSpanListForThread(DWORD processId, DWORD threadId, TSpanList& spanList, DWORD width, DWORD scale, DWORD* totalTime)
{
 if (RDTSCperSecond==0) return;

 __int64 count = 0;

 //show lastest samples
 //find starting sample index

 DWORD startIndex = samples.size()-1;
 __int64 period = RDTSCperSecond*scale/2;

 while (startIndex>0)
  {
   if (samples[samples.size()-1].RDTSC-samples[startIndex].RDTSC>period) break;
   startIndex--;
  }

 DWORD i = startIndex;
 while (i<samples.size()-1)
  {
   if ((threadId==OTHER_THREADS && samples[i].toProcessId != processId && samples[i].toProcessId!=0) ||
       (samples[i].toProcessId == processId && samples[i].toThreadId == threadId))
        {
									__int64 start = samples[i].RDTSC-samples[startIndex].RDTSC;
									__int64 end;

         DWORD j = i+1;
         while (j<samples.size())
          {
           end = samples[j].RDTSC-samples[startIndex].RDTSC;

           if ((threadId==OTHER_THREADS && samples[j].fromProcessId != processId && samples[j].fromProcessId!=0) ||
           (samples[j].fromProcessId == processId && samples[j].fromThreadId == threadId))
            {
             break;
            }
           j++;
          }

          if (end<start) end=start;

          if (start>RDTSCperSecond*scale/2)
           {
            break;
           }

          if (end>RDTSCperSecond*scale/2)
           {
            end = RDTSCperSecond*scale/2;
           }

          count+=end-start;

          start = start*width*2/RDTSCperSecond/scale;
          end = end*width*2/RDTSCperSecond/scale;

          if (start>end)
           {
            MessageBeep(0);
           }

          if (start>=width) break;

          if (end==start) end++;

          if (end>width-1) end=width-1;

          //try to merge with previous span
          if (spanList.size()>0)
           {
            if (spanList[spanList.size()-1].end >= (WORD)start)
             {
              spanList[spanList.size()-1].end = (WORD)end;
             }
              else
             {
              TSpan span;
              span.start = (WORD)start;
              span.end = (WORD)end;
              spanList.push_back(span);
             }
           }
            else
           {
            TSpan span;
            span.start = (WORD)start;
            span.end = (WORD)end;
            spanList.push_back(span);
           }
         }
   i++;
  }

  __int64 s = RDTSCperSecond*scale/2;
 *totalTime = (DWORD)((count*1000+s/2)/s);
}
#endif


//============================================================
//void TThreadSampler::EnumerateThreads()
//============================================================
void TThreadSampler::EnumerateThreads(DWORD processId)
{
 threads.clear();

 HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

 if (h != INVALID_HANDLE_VALUE)
 {
  THREADENTRY32 te;
  te.dwSize = sizeof(te);
  if (Thread32First(h, &te))
   {
    do
    {
     if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
      {
       if (te.th32OwnerProcessID==processId)
        {
         threads.push_back(te.th32ThreadID);
        }
      }
     te.dwSize = sizeof(te);
    } while (Thread32Next(h, &te));
  }
  CloseHandle(h);
 }
}

//============================================================
//const char* TThreadSampler::GetThreadName()
//============================================================
const char* TThreadSampler::GetThreadName(DWORD threadId)
{
	if (threadId == OTHER_THREADS) return "System/Other";

 TThreadNames::iterator iter = threadNames.find(threadId);
 if (iter==threadNames.end())
  {
   return "Unknown";
  }
   else
  {
   return iter->second;
  }
}

//============================================================
//void TThreadSampler::EnumerateProcesses()
//============================================================
void TThreadSampler::EnumerateProcesses()
{
 processes.clear();

 HANDLE hSnapshoot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

 if (hSnapshoot == INVALID_HANDLE_VALUE) return;

 PROCESSENTRY32 pe32;

 pe32.dwSize = sizeof(PROCESSENTRY32);

 if (Process32First(hSnapshoot, &pe32))
  {
   do
    {
     TProcessItem item;
     item.processId = pe32.th32ProcessID;
     strncpy(item.exeName,pe32.szExeFile,MAX_PATH);
     item.exeName[MAX_PATH-1]=0;
     processes.push_back(item);
    } while (Process32Next(hSnapshoot, &pe32));
  }

 CloseHandle (hSnapshoot);
}

int sort_function(const void *a, const void *b)
{
 __int64* tickA = &(((TThreadSampler::TSample*)a)->RDTSC);
 __int64* tickB = &(((TThreadSampler::TSample*)b)->RDTSC);

 if (*tickA>*tickB) return 1;
  else
 if (*tickA<*tickB) return -1;
  else
   return 0;
}

#ifdef TIMERSAMPLER
//============================================================
//void TThreadSampler::MergeCPUData()
//============================================================
void TThreadSampler::MergeCPUData(TSamplesCPU* CPUData)
{
 samples.driverData.index=0;

 DWORD count = 0;

 for (DWORD k=0; k<CPU_SAMPLES_COUNT; k++)
  {
   for (DWORD i=0; i<MAX_CPU_COUNT; i++)
    {
     samples[count]=CPUData[i][k];
     count++;
    }
  }

 assert(count==DRIVERSAMPLESCOUNT);

 qsort((void*)&samples.driverData.samples[0],DRIVERSAMPLESCOUNT,sizeof(TSample),sort_function);
}
#endif



