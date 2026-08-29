#ifndef _AUTODETECT_CPU_TESTSUITE_
#define _AUTODETECT_CPU_TESTSUITE_

#pragma once


#if defined(WIN32) || defined(WIN64)


class CCPUTestSuite
{
public:
	int RunTest();
};


#endif // #if defined(WIN32) || defined(WIN64)


#endif // #ifndef _AUTODETECT_CPU_TESTSUITE_