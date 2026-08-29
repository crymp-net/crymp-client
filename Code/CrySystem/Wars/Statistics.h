#ifndef __statistics_h__
#define __statistics_h__

#if defined(CRYMP_EMBEDDED_CRYSYSTEM)
inline void RegisterEngineStatistics() {}
#else
extern void RegisterEngineStatistics();
#endif

#endif //__statistics_h__
