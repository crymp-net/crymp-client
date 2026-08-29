/*=============================================================================
  CPUDetect.cpp : CPU detection.
  Copyright 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honitch Andrey

=============================================================================*/

#include "StdAfx.h"
#include "System.h"
#include "AutoDetectSpec.h"

#if defined(WIN32) || defined(WIN64)
#include <intrin.h>
#endif

/* features */
#define FPU_FLAG  0x0001
#define SERIAL_FLAG 0x40000
#define MMX_FLAG  0x800000
#define ISSE_FLAG 0x2000000

static int sSignature;

#if defined(LINUX) && defined(i386)
static void __cpuid(int *info, const unsigned int op)
{
	// you have to be careful; gcc doesn't like when you clobber ebx.
	//  so we'll manage the registers and array from inside the asm itself. :/
	__asm__ __volatile__ (
		"pushl %%ebx \n\t"
		"pushl %%ecx \n\t"
		"pushl %%edx \n\t"
		"cpuid \n\t"
		"movl %%eax,0(%%edi) \n\t"
		"movl %%ebx,4(%%edi) \n\t"
		"movl %%ecx,8(%%edi) \n\t"
		"movl %%edx,12(%%edi) \n\t"
		"popl %%edx \n\t"
		"popl %%ecx \n\t"
		"popl %%ebx \n\t" :
		/* no assignment, asm writes to (info) */ :
		"a" (op), "D" (info) :
		"cc", "memory"
	);
}
#endif

static double measure_clock_speed(double& SecondsPerCycle )
{
#if !defined(_XBOX) && !defined(LINUX) && !defined(PS3)

	LARGE_INTEGER Freq;
	LARGE_INTEGER c0, c1;
  
  DWORD priority_class;
  int   thread_priority;

  /* get a copy of the current thread and process priorities */
  priority_class = GetPriorityClass( GetCurrentProcess() );
  thread_priority = GetThreadPriority( GetCurrentThread() );

  /* make this thread the highest possible priority so we get the best timing */
  SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
  SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );

  
  QueryPerformanceFrequency(&Freq);

  double Frequency = double((INT64)(Freq.HighPart)*((INT64)65536*(INT64)65536)+(INT64)Freq.LowPart);
  assert (Frequency!=0);

  QueryPerformanceCounter(&c0);
  int64 ts0 = CryGetTicks();
  Sleep(100);
  int64 ts1 = CryGetTicks();
  QueryPerformanceCounter(&c1);

  double Count0 = double((INT64)(c0.HighPart)*((INT64)65536*(INT64)65536)+(INT64)c0.LowPart) / Frequency;
  double Count1 = double((INT64)(c1.HighPart)*((INT64)65536*(INT64)65536)+(INT64)c1.LowPart) / Frequency;

  SecondsPerCycle = (Count1-Count0) / (ts1-ts0);

  /* restore the thread priority */
  SetPriorityClass( GetCurrentProcess(), priority_class );
  SetThreadPriority( GetCurrentThread(), thread_priority );

#else
	SecondsPerCycle = 1.0; // just so this isn't a division by zero below.
#endif

  return 1.0e-6/SecondsPerCycle;
}

bool HasCPUID()
{
#if defined(WIN32) && !defined(WIN64)
	_asm
	{
		pushfd								// save EFLAGS
		pop eax								// store EFLAGS in EAX
		mov ebx, eax					// save in EBX for later testing
		xor eax, 00200000h		// toggle bit 21
		push eax							// push to stack
		popfd									// save changed EAX to EFLAGS
		pushfd								// push EFLAGS to TOS
		pop eax								// store EFLAGS in EAX
		cmp eax, ebx					// see if bit 21 has changed
		mov eax, 0						// clear eax to return "false"
		jz QUIT								// if no change, no CPUID
		mov eax, 1						// set eax to return "true"
QUIT:
	}
#elif defined(LINUX)
	// technically you might not have CPUID, but c'mon, really?
	return true;
#elif defined(WIN64) || defined(LINUX64)
	// we can safely assume CPUID exists on these 64-bit platforms
	return true;
#else
	return false;
#endif
}

bool IsAMD()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))
	int CPUInfo[4];
	char refID[] = "AuthenticAMD";		
	__cpuid( CPUInfo, 0x00000000 );
	if( ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2] )
		return true;
	else
		return false;
#else
	return false;
#endif
}

bool IsIntel()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))
	int CPUInfo[4];
	char refID[] = "GenuineIntel";		
	__cpuid( CPUInfo, 0x00000000 );
	if( ((int*) refID)[0] == CPUInfo[1] && ((int*) refID)[1] == CPUInfo[3] && ((int*) refID)[2] == CPUInfo[2] )
		return true;
	else
		return false;
#else
	return false;
#endif
}

bool Has64bitExtension()
{
#if defined(WIN32) && !defined(WIN64)	|| (defined(LINUX) && defined(i386))
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x80000001 ); // Argument "Processor Signature and AMD Features" 
	if( CPUInfo[3] & 0x20000000 )		// Bit 29 in edx is set if 64-bit address extension is supported
		return true;
	else
		return false;
#elif defined(WIN64) || defined(LINUX64)	
	return true;
#else
	return false;
#endif
}

bool HTSupported()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000001 );
	if( CPUInfo[3] & 0x10000000 )		// Bit 28 in edx is set if HT is supported
		return true;
	else
		return false;
#else
	return false;
#endif
}

/*
//////////////////////////////////////////////////////////////////////////
bool IsMultiCoreCPU()
{
#if defined(WIN32) || defined(WIN64)	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000000 );
	if( CPUInfo[0] != 0)		// Bit 28 in edx is set if HT is supported
		return true;
	else
		return false;
#else
	return false;
#endif
}

//////////////////////////////////////////////////////////////////////////
int GetCoresPerCPU()
{
#if defined(WIN32) || defined(WIN64)	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000004 );
	return CPUInfo[0] + 1;
#else
	return 1;
#endif
}
*/

uint8 LogicalProcPerPhysicalProc()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000001 );
	// Bits 16-23 in ebx contain the number of logical processors per physical processor when execute cpuid with eax set to 1
	return (uint8) ((CPUInfo[1] & 0x00FF0000) >> 16); 
#else
	return 1;
#endif
}

uint8 GetAPIC_ID()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000001 );
	// Bits 24-31 in ebx contain the unique initial APIC ID for the processor this code is running on. Default value = 0xff if HT is not supported.
	return (uint8) ((CPUInfo[1] & 0xFF000000) >> 24); 
#else
	return 0;
#endif
}

void GetCPUName( char* pName )
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	if( pName )
	{
		int CPUInfo[4];
		__cpuid( CPUInfo, 0x80000000 );
		if( CPUInfo[0] >= 0x80000004 )
		{
			__cpuid( CPUInfo, 0x80000002 );
			((int*)pName)[0] = CPUInfo[0];
			((int*)pName)[1] = CPUInfo[1];
			((int*)pName)[2] = CPUInfo[2];
			((int*)pName)[3] = CPUInfo[3];

			__cpuid( CPUInfo, 0x80000003 );
			((int*)pName)[4] = CPUInfo[0];
			((int*)pName)[5] = CPUInfo[1];
			((int*)pName)[6] = CPUInfo[2];
			((int*)pName)[7] = CPUInfo[3];

			__cpuid( CPUInfo, 0x80000004 );
			((int*)pName)[8] = CPUInfo[0];
			((int*)pName)[9] = CPUInfo[1];
			((int*)pName)[10] = CPUInfo[2];
			((int*)pName)[11] = CPUInfo[3];
		}
		else
			pName[0] = '\0';
	}
#else
	if( pName )
		pName[0] = '\0';
#endif
}

bool HasFPUOnChip()
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000001 );
	// Bit 0 in edx indicates presents of on chip FPU
	return (CPUInfo[3] & 0x00000001) != 0;
#else
	return false;
#endif
}

void GetCPUSteppingModelFamily(int& stepping, int& model, int& family)
{
#if defined(WIN32) || defined(WIN64) || (defined(LINUX) && defined(i386))	
	int CPUInfo[4];
	__cpuid( CPUInfo, 0x00000001 );	
	stepping = CPUInfo[0] & 0xF; // Bit 0-3 in eax specifies stepping
	model = (CPUInfo[0]>>4) & 0xF; // Bit 4-7 in eax specifies model
	family = (CPUInfo[0]>>8) & 0xF; // Bit 8-11 in eax specifies family
#else
	stepping = 0;
	model = 0;
	family = 0;
#endif
}

/* TODO: Alpha AXP */
static unsigned long __stdcall DetectProcessor( void *arg )
{
  const unsigned char hex_chars[16] =
  {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
  };
  unsigned long signature = 0;
  unsigned long cache_temp;
  unsigned long cache_eax = 0;
  unsigned long cache_ebx = 0;
  unsigned long cache_ecx = 0;
  unsigned long cache_edx = 0;
  unsigned long features_ebx = 0;
  unsigned long features_ecx = 0;
  unsigned long features_edx = 0;
  unsigned long serial_number[3];
  unsigned char cpu_type;
  unsigned char fpu_type;
  unsigned char CPUID_flag = 0;
  unsigned char celeron_flag = 0;
  unsigned char pentiumxeon_flag = 0;
  unsigned char amd3d_flag = 0;
  unsigned char name_flag = 0;
  unsigned char cyrix_flag = 0;
  char  vendor[13];
  char  name[49];
  char  *serial;
  const char  *cpu_string;
  const char  *cpu_extra_string;
  const char  *fpu_string;
  const char  *vendor_string;
  SCpu  *p = (SCpu *) arg;
  float     test_val[4] = { 1.f, 1.f, 1.f, 1.f };

	memset( p,0,sizeof(*p) );

	if (HasCPUID())
	{
		if (IsAMD() && Has64bitExtension())
		{
			p->meVendor = eCVendor_AMD;
			p->mFeatures |= CFI_3DNOW | CFI_SSE | CFI_SSE2;
			p->mbSerialPresent = false;
			strcpy(p->mSerialNumber,"");
			GetCPUSteppingModelFamily(p->mStepping, p->mModel, p->mFamily);
			strcpy( p->mVendor,"AMD" );
			GetCPUName( p->mCpuType );
			strcpy( p->mFpuType, HasFPUOnChip() ? "On-Chip" : "Unknown" );
			p->mSpeed = (int)measure_clock_speed(p->m_SecondsPerCycle);
			p->mbPhysical = true;

			return 1;
		}
		else if(IsIntel() && Has64bitExtension())
		{
			p->meVendor = eCVendor_Intel;
			p->mFeatures |= CFI_SSE | CFI_SSE2;
			p->mbSerialPresent = false;
			strcpy(p->mSerialNumber,"");
			GetCPUSteppingModelFamily(p->mStepping, p->mModel, p->mFamily);
			strcpy( p->mVendor,"Intel" );
			GetCPUName( p->mCpuType );
			strcpy( p->mFpuType, HasFPUOnChip() ? "On-Chip" : "Unknown" );
			p->mSpeed = (int)measure_clock_speed(p->m_SecondsPerCycle);

			p->mbPhysical = true;

			// Timur this check is somehow broken on Intel Core Duo
			/*
			// only check on 32 bits
			if (p->mStepping>4 && HTSupported())
				p->mbPhysical =  (GetAPIC_ID() & LogicalProcPerPhysicalProc()-1)==0;
				*/
			
			//if (IsMultiCoreCPU()) // Force to true for multi-core machines.
			{
				//p->mbPhysical = 1;
			}
			return 1;
		}
	}

#if (defined(WIN32) && defined(_CPU_X86))
	
	unsigned char v86_flag;
	unsigned short  fp_status;
  __asm
  {
    /*
     * 8086 processor check
     * Bits 12-15 of the FLAGS are always set on the
     * 8086 processor.
     */
    pushf         /* push original FLAGS */
    pop ax          /* get original FLAGS */
    mov cx, ax        /* save original FLAGS */
    and ax, 0fffh     /* clear bits 12-15 in FLAGS */
    push ax         /* save new FLAGS value on stack */
    popf          /* replace current FLAGS value */
    pushf         /* get new FLAGS */
    pop ax          /* store new FLAGS in AX */
    and ax, 0f000h      /* if bits 12-15 are set, then */
    cmp ax, 0f000h      /* processor is an 8086/8088 */
    mov cpu_type, 0     /* turn on 8086/8088 flag */
    jne check_80286     /* go check for 80286 */
    push sp         /* double check with push sp */
    pop dx          /* if value pushed was different */
    cmp dx, sp        /* means its really an 8086 */
    jne end_cpu_type    /* jump if processor is 8086/8088 */
    mov cpu_type, 010h    /* indicate unknown processor */
    jmp end_cpu_type

    /*
     * 286 processor check
     * Bits 12-15 of the FLAGS are always clear on the
     * Intel 286 processor in real-address mode.
     */
check_80286:
    smsw ax         /* save machine status word */
    and ax, 1       /* isolate PE bit of MSW */
    mov v86_flag, al    /* save PE bit to indicate V86 */
    or cx, 0f000h     /* try to set bits 12-15 */
    push cx         /* save new FLAGS value on stack */
    popf          /* replace current FLAGS value */
    pushf         /* get new FLAGS */
    pop ax          /* store new FLAGS in AX */
    and ax, 0f000h      /* if bits 12-15 are clear */
    mov cpu_type, 2     /* processor=80286, turn on 80286 flag */
    jz end_cpu_type     /* jump if processor is 80286 */

    /*
     * 386 processor check
     * The AC bit, bit #18, is a new bit introduced in the EFLAGS
     * on the 486 processor to generate alignment
     * faults.
     * This bit cannot be set on the 386 processor.
    */
    mov cpu_type, 3     /* turn on 80386 processor flag */
    pushfd          /* push original EFLAGS */
    pop eax         /* get original EFLAGS */
    mov ebx, eax      /* save original EFLAGS */
    xor eax, 040000h    /* flip AC bit in EFLAGS */
    push eax        /* save new EFLAGS value on stack */
    popfd         /* replace current EFLAGS value */
    pushfd          /* get new EFLAGS */
    pop eax         /* store new EFLAGS in EAX */
    cmp eax, ebx      /* cant toggle AC bit, processor=80386 */
    jz end_cpu_type     /* jump if 80386 processor */
    push ebx
    popfd         /* restore AC bit in EFLAGS */

    /*
     * Checking for ability to set/clear ID flag (Bit 21) in EFLAGS
     * which indicates the presence of a processor with the CPUID
     * instruction.
     */
    mov cpu_type, 4     /* turn on 80486 processor flag */
    pushfd            /* save EFLAGS to stack */
    pop eax           /* store EFLAGS in EAX */
    mov ebx, eax      /* save in EBX for testing later */
    xor eax, 0200000h   /* flip bit 21 in EFLAGS */
    push eax          /* save new EFLAGS value on stack */
    popfd             /* replace current EFLAGS value */
    pushfd            /* get new EFLAGS */
    pop eax           /* store new EFLAGS in EAX */
    cmp eax, ebx      /* see if bit 21 has changed */
    jne yes_CPUID     /* CPUID present */
    /* Cyrix processor check */
    xor ax, ax        /* clear ax */
    sahf            /* clear flags, bit 1 is always 1 in flags */
    mov ax, 5       /* move 5 into the dividend */
    mov bx, 2       /* move 2 into the divisor */
    div bl          /* do an operation that does not change flags */
    lahf            /* get flags */
    cmp ah, 2       /* check for change in flags */
    jne not_cyrix   /* flags changed, not a Cyrix CPU */
    mov cyrix_flag, 1
    /* TODO: identify cyrix processor */
not_cyrix:
    jmp end_cpu_type
yes_CPUID:
    push ebx
    popfd
    mov CPUID_flag, 1   /* flag indicating use of CPUID instruction */

    /*
     * Execute CPUID instruction to determine vendor, family,
     * model, stepping and features.
     */
    push ebx        /* save registers */
    push esi
    push edi
    xor eax, eax    /* set up for CPUID instruction */
    cpuid           /* get and save vendor ID */
    mov byte ptr[vendor], bl
    mov byte ptr[vendor + 1], bh
    ror ebx, 16
    mov byte ptr[vendor + 2], bl
    mov byte ptr[vendor + 3], bh
    mov byte ptr[vendor + 4], dl
    mov byte ptr[vendor + 5], dh
    ror edx, 16
    mov byte ptr[vendor + 6], dl
    mov byte ptr[vendor + 7], dh
    mov byte ptr[vendor + 8], cl
    mov byte ptr[vendor + 9], ch
    ror ecx, 16
    mov byte ptr[vendor + 10], cl
    mov byte ptr[vendor + 11], ch
    mov byte ptr[vendor + 12], 0
    cmp eax, 1        /* make sure 1 is valid input for CPUID */
    jl end_CPUID_type   /* if not, jump to end */
    mov eax, 1
    cpuid         /* get family/model/stepping/features */
    mov signature, eax
    mov features_ebx, ebx
    mov features_ecx, ecx
    mov features_edx, edx
    shr eax, 8        /* isolate family */
    and eax, 0fh
    mov cpu_type, al    /* set cpu_type with family */

    /*
     * Execute CPUID instruction to determine the cache descriptor
     * information.
     */
    xor eax, eax      /* set up to check the EAX value */
    cpuid
    cmp ax, 2       /* are cache descriptors supported? */
    jl end_CPUID_type
    mov eax, 2        /* set up to read cache descriptor */
    cpuid
    cmp al, 1       /* is one iteration enough to obtain */
    jne end_CPUID_type    /* cache information? */

    /* this code supports one iteration only. */
    mov cache_eax, eax    /* store cache information */
    mov cache_ebx, ebx    /* NOTE: for future processors, CPUID */
    mov cache_ecx, ecx    /* instruction may need to be run more */
    mov cache_edx, edx    /* than once to get complete cache information */
end_CPUID_type:
    pop edi         /* restore registers */
    pop esi
    pop ebx
    
    /* check for 3DNow! */
    mov eax, 080000000h   /* query for extended functions */
    cpuid         /* get extended function limit */
    cmp eax, 080000001h   /* functions up to 80000001h must be present */
    jb no_extended      /* 80000001h is not available */
    mov eax, 080000001h   /* setup extended function 1 */
    cpuid         /* call the function */
    test edx, 080000000h  /* test bit 31 */
    jz end_3dnow
    mov amd3d_flag, 1   /* 3DNow! supported */
end_3dnow:

    /* get CPU name */
    mov eax, 080000000h
    cpuid
    cmp eax, 080000004h
    jb end_name       /* functions up to 80000004h must be present */
    mov name_flag, 1
    mov eax, 080000002h
    cpuid
    mov dword ptr[name], eax
    mov dword ptr[name + 4], ebx
    mov dword ptr[name + 8], ecx
    mov dword ptr[name + 12], edx
    mov eax, 080000003h
    cpuid
    mov dword ptr[name + 16], eax
    mov dword ptr[name + 20], ebx
    mov dword ptr[name + 24], ecx
    mov dword ptr[name + 28], edx
    mov eax, 080000004h
    cpuid
    mov dword ptr[name + 32], eax
    mov dword ptr[name + 36], ebx
    mov dword ptr[name + 40], ecx
    mov dword ptr[name + 44], edx
end_name:

no_extended:

end_cpu_type:

    /* detect FPU */
    fninit          /* reset FP status word */
    mov fp_status, 05a5ah /* initialize temp word to non-zero */
    fnstsw fp_status    /* save FP status word */
    mov ax, fp_status   /* check FP status word */
    cmp al, 0       /* was correct status written */
    mov fpu_type, 0     /* no FPU present */
    jne end_fpu_type
    /* check control word */
    fnstcw fp_status    /* save FP control word */
    mov ax, fp_status   /* check FP control word */
    and ax, 103fh     /* selected parts to examine */
    cmp ax, 3fh       /* was control word correct */
    mov fpu_type, 0
    jne end_fpu_type    /* incorrect control word, no FPU */
    mov fpu_type, 1
    /* 80287/80387 check for the Intel386 processor */
    cmp cpu_type, 3
    jne end_fpu_type
    fld1          /* must use default control from FNINIT */
    fldz          /* form infinity */
    fdiv          /* 8087/Intel287 NDP say +inf = -inf */
    fld st          /* form negative infinity */
    fchs          /* Intel387 NDP says +inf <> -inf */
    fcompp          /* see if they are the same */
    fstsw fp_status     /* look at status from FCOMPP */
    mov ax, fp_status
    mov fpu_type, 2     /* store Intel287 NDP for FPU type */
    sahf          /* see if infinities matched */
    jz end_fpu_type     /* jump if 8087 or Intel287 is present */
    mov fpu_type, 3     /* store Intel387 NDP for FPU type */
end_fpu_type:
  }
#elif defined(LINUX) && defined(i386)
	CPUID_flag = 1;
	int info[4];
	__cpuid(info, 0);
	union
	{
		char str[16];
		int i[4];
	} vendorcvt;

  const bool have_cache_info = ((info[0] & 0xFFFF) >= 2);

  vendorcvt.i[0] = info[1];
  vendorcvt.i[1] = info[3];
  vendorcvt.i[2] = info[2];
  vendorcvt.i[3] = 0;
	strcpy(vendor, vendorcvt.str);
	
	__cpuid(info, 1);
	signature = (unsigned long) info[0];
	features_ebx = (unsigned long) info[1];
	features_ecx = (unsigned long) info[2];
	features_edx = (unsigned long) info[3];
	cpu_type = ((info[0] >> 8) & 0xf);
	
	if (have_cache_info)
	{
		__cpuid(info, 2);
		if ((info[0] & 0xF) == 1)  // is one iteration enough to obtain
		{
			cache_eax = info[0];
			cache_ebx = info[1];
			cache_ecx = info[2];
			cache_edx = info[3];
		}
	}
	
	__cpuid(info, 0x80000000);  // query for extended functions
	const int extfuncs = info[0];
	if (extfuncs < 0x80000001)
		amd3d_flag = 0;
	else
	{
		__cpuid(info, 0x80000001);  // setup extended function 1
		amd3d_flag = (info[3] == 0x80000000) ? 0 : 1;
	}
	
	name_flag = (extfuncs < 0x80000004) ? 0 : 1;
	if (name_flag)
	{
		__cpuid(info, 0x80000002);  // setup extended function 2
		memcpy(name, info, 16);
		__cpuid(info, 0x80000003);  // setup extended function 3
		memcpy(name+16, info, 16);
		__cpuid(info, 0x80000004);  // setup extended function 4
		memcpy(name+32, info, 16);
	}

	fpu_type = 3;  // 386 or better FPU (don't bother detecting).
#else //(defined(WIN32) && defined(_CPU_X86))
	cpu_type = 0xF;
  fpu_type = 3;
	signature = 0;
#endif //(defined(WIN32) && defined(_CPU_X86))
  p->mFamily = cpu_type;
  p->mModel = (signature >> 4) & 0xf;
  p->mStepping = signature & 0xf;

  p->mSpeed = (int)measure_clock_speed(p->m_SecondsPerCycle);
  p->mFeatures = 0;

  p->mFeatures |= amd3d_flag ? CFI_3DNOW : 0;
  p->mFeatures |= (features_edx & MMX_FLAG)  ? CFI_MMX : 0;
  p->mFeatures |= (features_edx & ISSE_FLAG) ? CFI_SSE : 0;
  p->mbSerialPresent = ((features_edx & SERIAL_FLAG) != 0);

  if( features_edx & SERIAL_FLAG )
  {
#if (defined(WIN32) && defined(_CPU_X86))
		/* read serial number */
		__asm
		{
			mov eax, 1
			cpuid
			mov serial_number[2], eax /* top 32 bits are the processor signature bits */
			mov eax, 3
			cpuid
			mov serial_number[1], edx
			mov serial_number[0], ecx
		}
#else
		serial_number[0] = serial_number[1] = serial_number[2] = 0;
#endif

    /* format number */
    serial = p->mSerialNumber;

    serial[0] = hex_chars[(serial_number[2] >> 28) & 0x0f];
    serial[1] = hex_chars[(serial_number[2] >> 24) & 0x0f];
    serial[2] = hex_chars[(serial_number[2] >> 20) & 0x0f];
    serial[3] = hex_chars[(serial_number[2] >> 16) & 0x0f];

    serial[4] = '-';

    serial[5] = hex_chars[(serial_number[2] >> 12) & 0x0f];
    serial[6] = hex_chars[(serial_number[2] >>  8) & 0x0f];
    serial[7] = hex_chars[(serial_number[2] >>  4) & 0x0f];
    serial[8] = hex_chars[(serial_number[2] >>  0) & 0x0f];

    serial[9] = '-';

    serial[10] = hex_chars[(serial_number[1] >> 28) & 0x0f];
    serial[11] = hex_chars[(serial_number[1] >> 24) & 0x0f];
    serial[12] = hex_chars[(serial_number[1] >> 20) & 0x0f];
    serial[13] = hex_chars[(serial_number[1] >> 16) & 0x0f];

    serial[14] = '-';

    serial[15] = hex_chars[(serial_number[1] >> 12) & 0x0f];
    serial[16] = hex_chars[(serial_number[1] >>  8) & 0x0f];
    serial[17] = hex_chars[(serial_number[1] >>  4) & 0x0f];
    serial[18] = hex_chars[(serial_number[1] >>  0) & 0x0f];

    serial[19] = '-';

    serial[20] = hex_chars[(serial_number[0] >> 28) & 0x0f];
    serial[21] = hex_chars[(serial_number[0] >> 24) & 0x0f];
    serial[22] = hex_chars[(serial_number[0] >> 20) & 0x0f];
    serial[23] = hex_chars[(serial_number[0] >> 16) & 0x0f];

    serial[24] = '-';

    serial[25] = hex_chars[(serial_number[0] >> 12) & 0x0f];
    serial[26] = hex_chars[(serial_number[0] >>  8) & 0x0f];
    serial[27] = hex_chars[(serial_number[0] >>  4) & 0x0f];
    serial[28] = hex_chars[(serial_number[0] >>  0) & 0x0f];

    serial[29] = 0;
  }

  vendor_string = "Unknown";
  cpu_string = "Unknown";
  cpu_extra_string = "";
  fpu_string = "Unknown";

  if( !CPUID_flag )
  {
    switch( cpu_type )
    {
      case 0:
        cpu_string = "8086";
        break;

      case 2:
        cpu_string = "80286";
        break;

      case 3:
        cpu_string = "80386";
        switch( fpu_type )
        {
          case 2:
            fpu_string = "80287";
            break;

          case 1:
            fpu_string = "80387";
            break;

          default:
            fpu_string = "None";
          break;
        }
        break;

      case 4:
        if( fpu_type )
        {
          cpu_string = "80486DX, 80486DX2 or 80487SX";
          fpu_string = "on-chip";
        }
        else
          cpu_string = "80486SX";
        break;
    }
  }
  else
  {    /* using CPUID instruction */
    if( !name_flag )
    {
      if( !strcmp(vendor, "GenuineIntel") )
      {
        vendor_string = "Intel";
        switch( cpu_type )
        {
          case 4:
            switch( p->mModel )
            {
              case 0:
              case 1:
                cpu_string = "80486DX";
                break;

              case 2:
                cpu_string = "80486SX";
                break;

              case 3:
                cpu_string = "80486DX2";
                break;

              case 4:
                cpu_string = "80486SL";
                break;

              case 5:
                cpu_string = "80486SX2";
                break;

              case 7:
                cpu_string = "Write-Back Enhanced 80486DX2";
                break;

              case 8:
                cpu_string = "80486DX4";
                break;

              default:
                cpu_string = "80486";
            }
            break;

          case 5:
            switch( p->mModel )
            {
              default:
              case 1:
              case 2:
              case 3:
                cpu_string = "Pentium";
                break;

              case 4:
                cpu_string = "Pentium MMX";
                break;
            }
            break;

          case 6:
            switch( p->mModel )
            {
              case 1:
                cpu_string = "Pentium Pro";
                break;

              case 3:
                cpu_string = "Pentium II";
                break;

              case 5:
              case 7:
                {
                  cache_temp = cache_eax & 0xFF000000;
                  if( cache_temp == 0x40000000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44000000) && (cache_temp <= 0x45000000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_eax & 0xFF0000;
                  if( cache_temp == 0x400000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x440000) && (cache_temp <= 0x450000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_eax & 0xFF00;
                  if( cache_temp == 0x4000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x4400) && (cache_temp <= 0x4500) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ebx & 0xFF000000;
                  if( cache_temp == 0x40000000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44000000) && (cache_temp <= 0x45000000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ebx & 0xFF0000;
                  if( cache_temp == 0x400000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x440000) && (cache_temp <= 0x450000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ebx & 0xFF00;
                  if( cache_temp == 0x4000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x4400) && (cache_temp <= 0x4500) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ebx & 0xFF;
                  if( cache_temp == 0x40 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44) && (cache_temp <= 0x45) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ecx & 0xFF000000;
                  if( cache_temp == 0x40000000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44000000) && (cache_temp <= 0x45000000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ecx & 0xFF0000;
                  if( cache_temp == 0x400000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x440000) && (cache_temp <= 0x450000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ecx & 0xFF00;
                  if( cache_temp == 0x4000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x4400) && (cache_temp <= 0x4500) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_ecx & 0xFF;
                  if( cache_temp == 0x40 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44) && (cache_temp <= 0x45) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_edx & 0xFF000000;
                  if( cache_temp == 0x40000000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44000000) && (cache_temp <= 0x45000000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_edx & 0xFF0000;
                  if( cache_temp == 0x400000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x440000) && (cache_temp <= 0x450000) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_edx & 0xFF00;
                  if( cache_temp == 0x4000 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x4400) && (cache_temp <= 0x4500) )
                    pentiumxeon_flag = 1;
                  cache_temp = cache_edx & 0xFF;
                  if( cache_temp == 0x40 )
                    celeron_flag = 1;
                  if( (cache_temp >= 0x44) && (cache_temp <= 0x45) )
                    pentiumxeon_flag = 1;

                  if( celeron_flag )
                  {
                    cpu_string = "Celeron";
                  }
                  else
                  {
                    if(pentiumxeon_flag)
                    {
                      if( p->mModel == 5 )
                      {
                        cpu_string = "Pentium II Xeon";
                      }
                      else
                      {
                        cpu_string = "Pentium III Xeon";
                      }
                    }
                    else
                    {
                      if( p->mModel == 5 )
                      {
                        cpu_string = "Pentium II";
                      }
                      else
                      {
                        cpu_string = "Pentium III";
                      }
                    }
                  }
                }
                break;

              case 6:
                cpu_string = "Celeron";
                break;

              case 8:
                cpu_string = "Pentium III";
                break;
            }
            break;

        }

        if( signature & 0x1000 )
        {
          cpu_extra_string = " OverDrive";
        }
        else
        if( signature & 0x2000 )
        {
          cpu_extra_string = " dual upgrade";
        }
      }
      else
      if( !strcmp( vendor, "CyrixInstead" ) )
      {
        vendor_string = "Cyrix";
        switch( p->mFamily )
        {
          case 4:
            switch( p->mModel )
            {
              case 4:
                cpu_string = "MediaGX";
                break;
            }
            break;

          case 5:
            switch( p->mModel )
            {
              case 2:
                cpu_string = "6x86";
                break;

              case 4:
                cpu_string = "GXm";
                break;
            }
            break;

          case 6:
            switch( p->mModel )
            {
              case 0:
                cpu_string = "6x86MX";
                break;
            }
            break;
        }
      }
      else
      if( !strcmp(vendor, "AuthenticAMD") )
      {
        strncpy( p->mVendor, "AMD", sizeof(p->mVendor) );
        switch( p->mFamily )
        {
          case 4:
            cpu_string = "Am486 or Am5x86";
            break;

          case 5:
            switch( p->mModel )
            {
              case 0:
              case 1:
              case 2:
              case 3:
                cpu_string = "K5";
                break;

              case 4:
              case 5:
              case 6:
              case 7:
                cpu_string = "K6";
                break;

              case 8:
                cpu_string = "K6-2";
                break;

              case 9:
                cpu_string = "K6-III";
                break;
            }
            break;

          case 6:
            cpu_string = "Athlon";
            break;
        }
      }
      else
      if( !strcmp(vendor, "CentaurHauls") )
      {
        vendor_string = "Centaur";
        switch( cpu_type )
        {
          case 5:
            switch( p->mModel )
            {
              case 4:
                cpu_string = "WinChip";
                break;

              case 8:
                cpu_string = "WinChip2";
                break;
            }
            break;
        }
      }
      else
      if( !strcmp(vendor, "UMC UMC UMC ") )
      {
        vendor_string = "UMC";
      }
      else
      if( !strcmp(vendor, "NexGenDriven") )
      {
        vendor_string = "NexGen";
      }
    }
    else
    {
      vendor_string = vendor;
      cpu_string = name;
    }

    if( features_edx & FPU_FLAG )
    {
      fpu_string = "On-Chip";
    }
    else
    {
      fpu_string = "Unknown";
    }
  }

  strncpy( p->mCpuType, cpu_string, sizeof(p->mCpuType) );
  strncat( p->mCpuType, cpu_extra_string, sizeof(p->mCpuType) );
  strncpy( p->mFpuType, fpu_string, sizeof(p->mFpuType) );
  strncpy( p->mVendor, vendor_string, sizeof(p->mVendor) );
  sSignature = signature;
  
  if (!stricmp(vendor_string, "Intel"))
    p->meVendor = eCVendor_Intel;
  else
  if (!stricmp(vendor_string, "Cyrix"))
    p->meVendor = eCVendor_Cyrix;
  else
  if (!stricmp(vendor_string, "AMD"))
    p->meVendor = eCVendor_AMD;
  else
  if (!stricmp(vendor_string, "Centaur"))
    p->meVendor = eCVendor_Centaur;
  else
  if (!stricmp(vendor_string, "NexGen"))
    p->meVendor = eCVendor_NexGen;
  else
  if (!stricmp(vendor_string, "UMC"))
    p->meVendor = eCVendor_UMC;
  else
    p->meVendor = eCVendor_Unknown;

  if (strstr(cpu_string, "8086"))
    p->meModel = eCpu_8086;
  else
  if (strstr(cpu_string, "80286"))
    p->meModel = eCpu_80286;
  else
  if (strstr(cpu_string, "80386"))
    p->meModel = eCpu_80386;
  else
  if (strstr(cpu_string, "80486"))
    p->meModel = eCpu_80486;
  else
  if (!stricmp(cpu_string, "Pentium MMX") || !stricmp(cpu_string, "Pentium"))
    p->meModel = eCpu_Pentium;
  else
  if (!stricmp(cpu_string, "Pentium Pro"))
    p->meModel = eCpu_PentiumPro;
  else
  if (!stricmp(cpu_string, "Pentium II"))
    p->meModel = eCpu_Pentium2;
  else
  if (!stricmp(cpu_string, "Pentium III"))
    p->meModel = eCpu_Pentium3;
  else
  if (!stricmp(cpu_string, "Pentium 4"))
    p->meModel = eCpu_Pentium4;
  else
  if (!stricmp(cpu_string, "Celeron"))
    p->meModel = eCpu_Celeron;
  else
  if (!stricmp(cpu_string, "Pentium II Xeon"))
    p->meModel = eCpu_Pentium2Xeon;
  else
  if (!stricmp(cpu_string, "Pentium III Xeon"))
    p->meModel = eCpu_Pentium3Xeon;
  else
  if (!stricmp(cpu_string, "MediaGX"))
    p->meModel = eCpu_CyrixMediaGX;
  else
  if (!stricmp(cpu_string, "6x86"))
    p->meModel = eCpu_Cyrix6x86;
  else
  if (!stricmp(cpu_string, "GXm"))
    p->meModel = eCpu_CyrixGXm;
  else
  if (!stricmp(cpu_string, "6x86MX"))
    p->meModel = eCpu_Cyrix6x86MX;
  else
  if (!stricmp(cpu_string, "Am486 or Am5x86"))
    p->meModel = eCpu_Am5x86;
  else
  if (!stricmp(cpu_string, "K5"))
    p->meModel = eCpu_AmK5;
  else
  if (!stricmp(cpu_string, "K6"))
    p->meModel = eCpu_AmK6;
  else
  if (!stricmp(cpu_string, "K6-2"))
    p->meModel = eCpu_AmK6_2;
  else
  if (!stricmp(cpu_string, "K6-III"))
    p->meModel = eCpu_AmK6_3;
  else
  if (!stricmp(cpu_string, "Athlon"))
    p->meModel = eCpu_AmAthlon;
  else
  if (!stricmp(cpu_string, "Duron"))
    p->meModel = eCpu_AmDuron;
  else
  if (!stricmp(cpu_string, "WinChip"))
    p->meModel = eCpu_CenWinChip;
  else
  if (!stricmp(cpu_string, "WinChip2"))
    p->meModel = eCpu_CenWinChip2;
  else
    p->meModel = eCpu_Unknown;

	p->mbPhysical = true;
	if (!strcmp(vendor_string,"GenuineIntel") && p->mStepping>4 && HTSupported())
		p->mbPhysical =  (GetAPIC_ID() & LogicalProcPerPhysicalProc()-1)==0;

  return 1;
}

/*
#ifdef WIN64
typedef BOOL (WINAPI *LPFN_GLPI)( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

int GetPhysicalCpuCount()
{
	BOOL done;
	BOOL rc;
	DWORD returnLength;
	DWORD procCoreCount;
	DWORD byteOffset;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer;
	LPFN_GLPI Glpi;

	Glpi = (LPFN_GLPI) GetProcAddress( GetModuleHandle(TEXT("kernel32")),"GetLogicalProcessorInformation");
	if (NULL == Glpi) 
	{
		return (1);
	}

	done = FALSE;
	buffer = NULL;
	returnLength = 0;

	while (!done) 
	{
		rc = Glpi(buffer, &returnLength);

		if (FALSE == rc) 
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
			{
				if (buffer) 
					free(buffer);

				buffer=(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

				if (NULL == buffer) 
				{
					return (1);
				}
			} 
			else 
			{
				return (1);
			}
		} 
		else done = TRUE;
	}

	procCoreCount = 0;
	byteOffset = 0;

	while (byteOffset < returnLength) 
	{
		switch (buffer->Relationship) 
		{
		case RelationProcessorCore:
			procCoreCount++;
			break;

		default:
			break;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		buffer++;
	}

	free (buffer);

	return procCoreCount;
}
#else //WIN64
int GetPhysicalCpuCount()
{
	return 1;
}
#endif //WIN64
*/

// #define SQRT_TEST

#ifdef SQRT_TEST
/* ------------------------------------------------------------------------------ */
float Null(float f) { return f; } 
float Inv(float f) { return 1.f/f; } 

float Square(float f) { return f*f; }
float InvSquare(float f) { return 1.f/(f*f); }

float Sqrt(float f) { return sqrtf(f); } 
float SqrtT(float f) { return sqrt_tpl(f); }
float SqrtFT(float f) { return sqrt_fast_tpl(f); }

float InvSqrt(float f) { return 1.f/sqrtf(f); } 
float ISqrtT(float f) { return isqrt_tpl(f); }
float ISqrtFT(float f) { return isqrt_fast_tpl(f); }

float SSEInv(float f) 
{ 
	__m128 s = _mm_rcp_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); return r;
} 
float SSESqrt(float f) 
{ 
	__m128 s = _mm_sqrt_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); return r;
} 
float SSEISqrt(float f) 
{ 
	__m128 s = _mm_sqrt_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); 
	return 1.f / r;
} 
float SSERSqrt(float f) 
{ 
	 __m128 s = _mm_rsqrt_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); return r;
} 
float SSERSqrtInv(float f)
{
	__m128 s = _mm_rcp_ss(_mm_rsqrt_ss(_mm_load_ss(&f)));
	float r; _mm_store_ss(&r, s); return r;
}
float SSERSqrtNR(float f) 
{ 
	__m128 s = _mm_rsqrt_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); 
	return CorrectInvSqrt(f, r);
} 
float SSERISqrtNR(float f) 
{ 
	__m128 s = _mm_rsqrt_ss(_mm_load_ss(&f));
	float r; _mm_store_ss(&r, s); 
	return 1.f / CorrectInvSqrt(f, r);
} 

inline float cryISqrtf(float fVal)
{
  unsigned int *n1 = (unsigned int *)&fVal;
  unsigned int n = 0x5f3759df - (*n1 >> 1);
  float *n2 = (float *)&n;
  fVal = (1.5f - (fVal * 0.5f) * *n2 * *n2) * *n2;
  return fVal;
}

float cryISqrtNRf(float f) 
{ 
	return CorrectInvSqrt(f, cryISqrtf(f)); 
} 

inline float crySqrtf(float fVal)
{
  return 1.0f / cryISqrtf(fVal);
}

/* ------------------------------------------------------------------------------ */
struct SMathTest
{
	typedef int64 TTime;
	static inline TTime GetTime()
	{
		return CryGetTicks();
	}

	static const int T = 100, N = 1000;
	float fNullTime;

	float aTestVals[T];
	float aResVals[T];

	typedef float (*FFloatFunc)(float f);

	float Timer(const char* sName, FFloatFunc func, FFloatFunc finv)
	{
		for (int i = 0; i < T; i++)
			aResVals[i] = func(aTestVals[i]);
		TTime tStart = GetTime();
		for (int r = 0; r < N; r++)
			for (int i = 0; i < T; i++)
				aResVals[i] = func(aTestVals[i]);
		float fTime = (GetTime() - tStart) / float(N*T);

		// Error computation.
		float fAvgErr = 0.f, fMaxErr = 0.f;
		for (int i = 0; i < T; i++)
		{
			float fErr = abs( finv(aResVals[i]) / aTestVals[i] - 1.f );
			fAvgErr += fErr;
			fMaxErr = max(fMaxErr, fErr);
		}
		fAvgErr /= float(T);

		CryLogAlways("%-20s : %5.2f cycles, avg err %.2e, max err %.2e", sName, fTime-fNullTime, fAvgErr, fMaxErr );

		return fTime;
	};

	SMathTest()
	{
		for (int i = 0; i < T; i++)
			aTestVals[i] = powf(Random(1.f, 2.f), Random(-30.f, 30.f));

		CryLogAlways("--- Math Test ---");

		fNullTime = 0.f;
		fNullTime = Timer("(null)", &Null, &Null);

		CryLogAlways("-- Inverse methods");
		Timer("1/f", &Inv, &Inv);
		Timer("rcpss", &SSEInv, &Inv);

		CryLogAlways("-- Sqrt methods");
		Timer("sqrtf()", &Sqrt, &Square);
		Timer("sqrt_tpl()", &SqrtT, &Square);
		Timer("sqrt_fast_tpl()", &SqrtFT, &Square);
		Timer("crySqrt()", &crySqrtf, &Square);

		// Timer("sqrtss", &SSESqrt, &Square);
		// Timer("rsqrtss,rcpss", &SSERSqrtInv, &Square);
		Timer("1/rsqrtss,correction", &SSERISqrtNR, &Square);

		CryLogAlways("-- InvSqrt methods");
		Timer("1/sqrtf()", &InvSqrt, &InvSquare);
		Timer("isqrt_tpl()", &ISqrtT, &InvSquare);
		Timer("isqrt_fast_tpl()", &ISqrtFT, &InvSquare);
		Timer("cryISqrt()", &cryISqrtf, &InvSquare);

		Timer("1/sqrtss", &SSEISqrt, &InvSquare);
		// Timer("rsqrtss", &SSERSqrt, &InvSquare);
		// Timer("rsqrtss,correction", &SSERSqrtNR, &InvSquare);
		Timer("cryISqrt,correction", &cryISqrtNRf, &InvSquare);

		CryLogAlways("--------------------");
	}
};

#endif // SQRT_TEST

/* ------------------------------------------------------------------------------ */
void CCpuFeatures::Detect(void)
{
  m_NumSystemProcessors = 1;
  m_NumAvailProcessors = 0;

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32
	CryLogAlways("\n--- CPU detection ---\n" );

  DWORD_PTR process_affinity_mask;
  DWORD_PTR thread_processor_mask = 1;
  process_affinity_mask = 1;

  /* get the system info to derive the number of processors within the system. */

	SYSTEM_INFO   sys_info;
	DWORD_PTR system_affinity_mask;
  GetSystemInfo( &sys_info );
  m_NumSystemProcessors = sys_info.dwNumberOfProcessors;
  m_NumAvailProcessors = 0;
  GetProcessAffinityMask( GetCurrentProcess(), &process_affinity_mask, &system_affinity_mask );

  for( unsigned char c = 0; c < m_NumSystemProcessors; c++ )
  {
    if( process_affinity_mask & ((DWORD_PTR)1 << c) )
    {
      m_NumAvailProcessors++;
			SetProcessAffinityMask( GetCurrentProcess(), 1 << c );
			DetectProcessor( &m_Cpu[c] );
			m_Cpu[c].mAffinityMask = ((DWORD_PTR)1 << c);
    }
  }

	SetProcessAffinityMask( GetCurrentProcess(), process_affinity_mask );

  m_bOS_ISSE = false;
  m_bOS_ISSE_EXCEPTIONS = false;
#elif defined(LINUX)
	CryLogAlways("\n--- CPU detection ---\n" );
	m_NumSystemProcessors = 0;
	
	// it would be nice if there were a syscall for this.
	//  parse out the text file /proc/cpuinfo ... this is actually
	//  a virtual file generated by the kernel. We look for a specific
	//  key that repeats once per CPU core, and increment a counter each
	//  time we see it.  --ryan.
	int fd = open("/proc/cpuinfo", O_RDONLY);
	if (fd != -1)
	{
		// seriously, if you have more than 256k, you have too many cores.  :)
		const size_t buflen = 1024 * 256;
		char *buf = new char[buflen];
		char *line = buf;
		size_t br = read(fd, buf, buflen);
		if (br > 0)
		{
			const char *key = "vendor_id\t:";
			const size_t keylen = strlen(key);
			buf[br] = '\0';  // null-terminate it.
			char *ptr = buf;
			while (*ptr)
			{
				if ((*ptr == '\r') || (*ptr == '\n'))
				{
					*ptr = '\0';
					if (strncmp(line, key, keylen) == 0)
						m_NumSystemProcessors++;
					line = ptr + 1;
				}
				ptr++;
			}
		}
		delete[] buf;
		close(fd);
	}
	
	if (m_NumSystemProcessors < 1)
		m_NumSystemProcessors = 1;  // oh well.
	
	for( unsigned char c = 0; c < m_NumSystemProcessors; c++ )
	{
		// !!! FIXME: query for CPU affinity...
		//if( process_affinity_mask & ((DWORD_PTR)1 << c) )
		{
			m_NumAvailProcessors++;
			//SetProcessAffinityMask( GetCurrentProcess(), 1 << c );
			DetectProcessor( &m_Cpu[c] );
			m_Cpu[c].mAffinityMask = ((DWORD_PTR)1 << c);
		}
	}
	
#elif defined(PS3) 
	m_NumSystemProcessors = 2;
	m_NumAvailProcessors = 2;
#endif // WIN32
 
#if !defined(PS3)
#if defined(WIN32) || defined(WIN64)
  CryLogAlways("Total number of logical processors: %d\n", m_NumSystemProcessors);
  CryLogAlways("Number of available logical processors: %d\n\n", m_NumAvailProcessors);
#else
	CryLogAlways("Number of system processors: %d\n", m_NumSystemProcessors);
	CryLogAlways("Number of available processors: %d\n\n", m_NumAvailProcessors);
#endif

	if (m_NumAvailProcessors > MAX_CPU)
    m_NumAvailProcessors = MAX_CPU;

  for (int i=0; i<m_NumAvailProcessors; i++)
  {
    SCpu *p = &m_Cpu[i];

    CryLogAlways("Processor %d:\n", i );
    CryLogAlways("CPU: %s %s\n", p->mVendor, p->mCpuType );
    CryLogAlways("Family: %d, Model: %d, Stepping: %d\n", p->mFamily, p->mModel, p->mStepping );
    CryLogAlways("FPU: %s\n", p->mFpuType );
    CryLogAlways("CPU Speed (estimated): %f MHz\n", 1.0e-6/p->m_SecondsPerCycle );
    if (p->mFeatures & CFI_MMX)
      CryLogAlways("MMX: present\n");
    else
      CryLogAlways("MMX: not present\n");
    if (p->mFeatures & CFI_SSE)
      CryLogAlways("SSE: present\n");
    else
      CryLogAlways("SSE: not present\n");
    if (p->mFeatures & CFI_3DNOW)
      CryLogAlways("3DNow!: present\n");
    else
      CryLogAlways("3DNow!: not present\n");
    if( p->mbSerialPresent )
      CryLogAlways("Serial number: %s\n\n", p->mSerialNumber );
    else
      CryLogAlways("Serial number not present or disabled\n\n" );
  }

#if defined(WIN32) || defined(WIN64)
	unsigned int numSysCores(1), numProcessCores(1);
	Win32SysInspect::GetNumCPUCores(numSysCores, numProcessCores);
	m_NumSystemProcessors = numSysCores;
	m_NumAvailProcessors = numProcessCores;
	CryLogAlways("Total number of system cores: %d\n", m_NumSystemProcessors);
	CryLogAlways("Number of cores available to process: %d\n", m_NumAvailProcessors);
#endif

#ifdef SQRT_TEST
	SMathTest test;
#endif

  CryLogAlways("---------------------" );
#endif//PS3
	//m_NumPhysicsProcessors = m_NumSystemProcessors;
	for(int i=m_NumPhysicsProcessors=0; i<m_NumAvailProcessors; i++) if (m_Cpu[i].mbPhysical)
		++m_NumPhysicsProcessors;
}

