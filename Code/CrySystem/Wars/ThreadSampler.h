//===========================================================================
// class TThreadSampler
// Class for threads execution monitoring
// Copyright (C) 2006 by Roman Lut
//===========================================================================

#ifndef ThreadSamplerH
#define ThreadSamplerH

#define TIMERSAMPLER

#include <Windows.h>
#include <vector>
#include <map>

#pragma pack(1)
//========================================================
// class TThreadSampler
//========================================================
class TThreadSampler
{
 public:

  //number of samles in driver cyclic buffer
#ifdef TIMERSAMPLER
  static const int MAX_CPU_COUNT=4;
  static const int CPU_SAMPLES_COUNT=4000;
  static const int DRIVERSAMPLESCOUNT=MAX_CPU_COUNT*CPU_SAMPLES_COUNT;
#else
  static const int DRIVERSAMPLESCOUNT=10000;
#endif

  //dummy ThreadId for CreateSpanListForThread() method
  static const DWORD OTHER_THREADS  = MAXDWORD;

  //we will request snapshot from driver once per 2 seconds
  static const DWORD SNAPSHOT_UPDATE_PERIOD  = 2000;

  //======================
  //TSample
  //======================
  //sample from driver
  typedef struct
  {
#ifdef TIMERSAMPLER
   DWORD processId;
   DWORD threadId;
#else
   DWORD fromProcessId;
   DWORD fromThreadId;
   DWORD toProcessId;
   DWORD toThreadId;
#endif
   __int64 RDTSC;
  } TSample;

  //======================
  //class TSamples
  //======================
  //class for holding snapshot of samples buffer, recevied from driver
  //CBuilder does not handle <int n> template well (wrong sizeof()),
  //se we have to use macro
#define DEFINE_TSAMPLES(n,m)                                        \
  class TSamples##m                                           \
  {                                                        \
   public:                                                 \
    DWORD size() const                                     \
    {                                                      \
     return n;                                             \
    }                                                      \
                                                           \
    DWORD memorySize() const                               \
    {                                                      \
     return n*sizeof(TSample)+sizeof(DWORD);               \
    }                                                      \
                                                           \
    TSample& operator[](DWORD index)                       \
    {                                                      \
     index = (index+driverData.index)%n;                   \
     return driverData.samples[index];                     \
    }                                                      \
                                                           \
    friend class TThreadSampler;                           \
                                                           \
   private:                                                \
				TSamples##m()                                          \
    {                                                      \
     memset(&driverData,0, sizeof(driverData));            \
    };                                                     \
                                                           \
				struct                                                 \
				{                                                      \
					DWORD index;                                          \
					TSample samples[n];                                   \
				} driverData;                                          \
  };

		DEFINE_TSAMPLES(DRIVERSAMPLESCOUNT,ALL)

#ifdef TIMERSAMPLER
		DEFINE_TSAMPLES(CPU_SAMPLES_COUNT,CPU)
#endif

  //======================
  //TSpan
  //======================
  //span list for graph, in pixels
  typedef struct
  {
   WORD start;
   WORD end;
  } TSpan;

  //======================
  //TProcessItem
  //======================
  typedef struct
  {
   DWORD processId;
   char exeName[MAX_PATH];
  } TProcessItem;


  typedef std::vector<TSpan> TSpanList;
  typedef std::vector<DWORD> TThreadsList;
  typedef std::vector<TProcessItem> TProcessesList;

  //Processes snapshot is built with EnumerateProcesses();
		TProcessesList processes;

  //Threads snapshot is built with EnumerateThreads();
  TThreadsList threads;

  //samples from driver
  TSamplesALL samples;

  TThreadSampler();
  ~TThreadSampler();

  //Update samples snapthot from driver
  //should be called at least once per two seconds
  //returns false if unable to communicate with driver
  //contextSwitches - average number of context switches per second (SwapContextHook() version only)
  bool MakeSnapshot(DWORD* contextSwitches);

  //Create span list for drawing graph
  //processId,threadId - build list for specified thread.
  //threadId = OTHER_THREADS - build list for threads not belonging to specified process
  //width - width of timeline, in pixels
  //scale - scale of timeline, in 0.5 seconds units, f.e 2 for 1-second timeline. 4 for 2 seconds timeline etc.
  //totalTime - receives total time in miliiseconds, used by this thread during one second (does not depend on 'scale' paraleter).
  void CreateSpanListForThread(DWORD processId,
                               DWORD threadId,
                               TSpanList& spanList,
                               DWORD width,
                               DWORD scale,
                               DWORD* totalTime);

  //get current Time Stamp Counter value from CPU1
  static __int64 RDTSC();


  static void SetThreadName(const char* name);
  static void SetThreadNameEx(DWORD threadId, const char* name);
  //can return "Unknown"
  static const char* GetThreadName(DWORD threadId);

		void EnumerateProcesses();
		void EnumerateThreads(DWORD processId);

		__int64 RDTSCperSecond;

 private:
  typedef std::map<DWORD, char*> TThreadNames;
		static TThreadNames threadNames;

  DWORD lastSnapshot;
  DWORD lastContextSwitches;

		__int64 lastRDTSC;
		__int64 lastPC;

		void LoadDriver();
		void UnloadDriver();
		void GetDriverFileName(char* fnme);
#ifdef TIMERSAMPLER
  void MergeCPUData(TSamplesCPU* CPUData);
#endif

};
#pragma pack()

#endif



