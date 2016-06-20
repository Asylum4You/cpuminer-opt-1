/**
 * Unit to read cpu informations
 *
 * tpruvot 2014
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "miner.h"

#ifndef WIN32

#define HWMON_PATH \
 "/sys/devices/platform/coretemp.0/hwmon/hwmon1/temp1_input"
#define HWMON_ALT \
 "/sys/class/hwmon/hwmon1/temp1_input"
#define HWMON_ALT2 \
 "/sys/class/hwmon/hwmon0/temp1_input"

static float linux_cputemp(int core)
{
	float tc = 0.0;
	FILE *fd = fopen(HWMON_PATH, "r");
	uint32_t val = 0;

	if (!fd)
		fd = fopen(HWMON_ALT, "r");

	if (!fd)
		fd = fopen(HWMON_ALT2, "r");

	if (!fd)
		return tc;

	if (fscanf(fd, "%d", &val))
		tc = val / 1000.0;

	fclose(fd);
	return tc;
}

#define CPUFREQ_PATH \
 "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq"
static uint32_t linux_cpufreq(int core)
{
	FILE *fd = fopen(CPUFREQ_PATH, "r");
	uint32_t freq = 0;

	if (!fd)
		return freq;

	if (!fscanf(fd, "%d", &freq))
		return freq;

	return freq;
}

#else /* WIN32 */

static float win32_cputemp(int core)
{
	// todo
	return 0.0;
}

#endif /* !WIN32 */


/* exports */


float cpu_temp(int core)
{
#ifdef WIN32
	return win32_cputemp(core);
#else
	return linux_cputemp(core);
#endif
}

uint32_t cpu_clock(int core)
{
#ifdef WIN32
	return 0;
#else
	return linux_cpufreq(core);
#endif
}

int cpu_fanpercent()
{
	return 0;
}

#ifndef __arm__
static inline void cpuid(int functionnumber, int output[4]) {
#if defined (_MSC_VER) || defined (__INTEL_COMPILER)
	// Microsoft or Intel compiler, intrin.h included
	__cpuidex(output, functionnumber, 0);
#elif defined(__GNUC__) || defined(__clang__)
	// use inline assembly, Gnu/AT&T syntax
	int a, b, c, d;
	asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(functionnumber), "c"(0));
	output[0] = a;
	output[1] = b;
	output[2] = c;
	output[3] = d;
#else
	// unknown platform. try inline assembly with masm/intel syntax
	__asm {
		mov eax, functionnumber
		xor ecx, ecx
		cpuid;
		mov esi, output
		mov[esi], eax
		mov[esi + 4], ebx
		mov[esi + 8], ecx
		mov[esi + 12], edx
	}
#endif
}
#else /* !__arm__ */
#define cpuid(fn, out) out[0] = 0;
#endif

// I presume an assembly function needs to be inline and
// requires a wrapper to work like a normal function
void processor_id ( int functionnumber, int output[4] )
{ cpuid( functionnumber, output ); }
  
void cpu_getname(char *outbuf, size_t maxsz)
{
   memset(outbuf, 0, maxsz);
#ifdef WIN32
   char brand[0xC0] = { 0 };
   int output[4] = { 0 }, ext;
   cpuid(0x80000000, output);
   ext = output[0];
   if (ext >= 0x80000004)
   {
      for (int i = 2; i <= (ext & 0xF); i++)
      {
         cpuid(0x80000000+i, output);
	 memcpy(&brand[(i-2) * 4*sizeof(int)], output, 4*sizeof(int));
      }
      snprintf(outbuf, maxsz, "%s", brand);
   }
   else
   {
      // Fallback, for the i7-5775C will output
      // Intel64 Family 6 Model 71 Stepping 1, GenuineIntel
      snprintf(outbuf, maxsz, "%s", getenv("PROCESSOR_IDENTIFIER"));
   }
#else
   // Intel(R) Xeon(R) CPU E3-1245 V2 @ 3.40GHz
   FILE *fd = fopen("/proc/cpuinfo", "rb");
   char *buf = NULL, *p, *eol;
   size_t size = 0;
   if (!fd) return;
   while(getdelim(&buf, &size, 0, fd) != -1)
   {
      if (buf && (p = strstr(buf, "model name\t")) && strstr(p, ":"))
      {
          p = strstr(p, ":");
          if (p)
          {
              p += 2;
	      eol = strstr(p, "\n"); if (eol) *eol = '\0';
	      snprintf(outbuf, maxsz, "%s", p);
          }
          break;
        }
   }
   free(buf);
   fclose(fd);
#endif
}

void cpu_getmodelid(char *outbuf, size_t maxsz)
{
   memset(outbuf, 0, maxsz);
#ifdef WIN32
   // For the i7-5775C will output 6:4701:8
   snprintf(outbuf, maxsz, "%s:%s:%s", getenv("PROCESSOR_LEVEL"), // hexa ?
   getenv("PROCESSOR_REVISION"), getenv("NUMBER_OF_PROCESSORS"));
#else
   FILE *fd = fopen("/proc/cpuinfo", "rb");
   char *buf = NULL, *p, *eol;
   int cpufam = 0, model = 0, stepping = 0;
   size_t size = 0;
   if (!fd) return;

   while(getdelim(&buf, &size, 0, fd) != -1)
   {
      if (buf && (p = strstr(buf, "cpu family\t")) && strstr(p, ":"))
      {
         p = strstr(p, ":");
	 if (p)
         {
	    p += 2;
	    cpufam = atoi(p);
	 }
      }
      if (buf && (p = strstr(buf, "model\t")) && strstr(p, ":"))
      {
         p = strstr(p, ":");
	 if (p)
         {
            p += 2;
            model = atoi(p);
         }
      }
      if (buf && (p = strstr(buf, "stepping\t")) && strstr(p, ":"))
      {
         p = strstr(p, ":");
         if (p)
         {
            p += 2;
            stepping = atoi(p);
         }
      }
      if (cpufam && model && stepping)
      {
         snprintf( outbuf, maxsz, "%x:%02x%02x:%d", cpufam, model, stepping,
                   num_cpus);
         outbuf[maxsz-1] = '\0';
         break;
      }
   }
   free(buf);
   fclose(fd);
#endif
}
 
// http://en.wikipedia.org/wiki/CPUID

// CPUID commands
#define VENDOR_ID                  (0)
#define PROCESSOR_INFO             (1)
#define CACHE_TLB_DESCRIPTOR       (2)
#define EXTENDED_FEATURES          (7)
#define HIGHEST_EXTENDED_FUNCTION  (0x80000000)
#define EXTENDED_PROCESSOR_INFO    (0x80000001)
#define PROCESSOR_BRAND_STRING_1   (0x80000002)
#define PROCESSOR_BRAND_STRING_2   (0x80000003)
#define PROCESSOR_BRAND_STRING_3   (0x80000004)

#define EAX_Reg  (0)
#define EBX_Reg  (1)
#define ECX_Reg  (2)
#define EDX_Reg  (3)

#define XSAVE_Flag    (1 << 26)
#define OSXSAVE_Flag  (1 << 27)
#define AVX1_Flag     (1 << 28)
#define XOP_Flag      (1 << 11)
#define FMA3_Flag     (1 << 12)
#define AES_Flag      (1 << 25)
#define SSE42_Flag    (1 << 20)

#define SSE_Flag      (1 << 25) // EDX
#define SSE2_Flag     (1 << 26) // EDX

#define AVX2_Flag     (1 << 5) // ADV EBX

// Use this to detect presence of feature
#define AVX1_mask     (AVX1_Flag|XSAVE_Flag|OSXSAVE_Flag)
#define FMA3_mask     (FMA3_Flag|AVX1_mask)

bool has_sse2()
{
#ifdef __arm__
    return false;
#else
    int cpu_info[4] = { 0 };
    cpuid( PROCESSOR_INFO, cpu_info );
    return cpu_info[ EDX_Reg ] & SSE2_Flag;
#endif
}

// nehalem and above, no AVX1 on nehalem
bool has_aes_ni()
{
#ifdef __arm__
	return false;
#else
	int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
	return cpu_info[ ECX_Reg ] & AES_Flag;
#endif
}

// westmere and above
bool has_avx1()
{
#ifdef __arm__
        return false;
#else
        int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
        return ( ( cpu_info[ ECX_Reg ] & AVX1_mask ) == AVX1_mask );
#endif
}

// haswell and above
bool has_avx2()
{
#ifdef __arm__
    return false;
#else
    int cpu_info[4] = { 0 };
    cpuid( EXTENDED_FEATURES, cpu_info );
    return cpu_info[ EBX_Reg ] & AVX2_Flag;
#endif
}

bool has_xop()
{
#ifdef __arm__
        return false;
#else
        int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
        return cpu_info[ ECX_Reg ] & XOP_Flag;
#endif
}

bool has_fma3()
{
#ifdef __arm__
        return false;
#else
        int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
        return ( ( cpu_info[ ECX_Reg ] & FMA3_mask ) == FMA3_mask );
#endif
}

bool has_sse42()
{
#ifdef __arm__
        return false;
#else
        int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
        return cpu_info[ ECX_Reg ] & SSE42_Flag;
#endif
}

bool has_sse()
{
#ifdef __arm__
        return false;
#else
        int cpu_info[4] = { 0 };
        cpuid( PROCESSOR_INFO, cpu_info );
        return cpu_info[ EDX_Reg ] & SSE_Flag;
#endif
}

uint32_t cpuid_get_highest_function_number()
{
  uint32_t cpu_info[4] = {0};
  cpuid( VENDOR_ID, cpu_info);
  return cpu_info[ EAX_Reg];
}

void cpuid_get_highest_function( char* s )
{
  uint32_t fn = cpuid_get_highest_function_number();
  switch (fn)
  {
    case 0x16:
      strcpy( s, "Skylake" );
      break;
    case 0xd:
      strcpy( s, "IvyBridge" );
      break;
    case 0xb:
      strcpy( s, "Corei7" );
      break;
    case 0xa:
      strcpy( s, "Core2" );
      break;
    default:
      sprintf( s, "undef %x", fn );
  }
}

void cpu_bestfeature(char *outbuf, int maxsz)
{
#ifdef __arm__
	sprintf(outbuf, "ARM");
#else
	int cpu_info[4] = { 0 };
	int cpu_info_adv[4] = { 0 };
	cpuid( PROCESSOR_INFO, cpu_info );
	cpuid( EXTENDED_FEATURES, cpu_info_adv );

        if ( has_avx1() && has_avx2() )
              sprintf(outbuf, "AVX2");
        else if ( has_avx1() )
              sprintf(outbuf, "AVX1");
        else if ( has_fma3() )
              sprintf(outbuf, "FMA3");
        else if ( has_xop() )
                sprintf(outbuf, "XOP");
        else if ( has_sse42() )
                sprintf(outbuf, "SSE42");
        else if ( has_sse2() )
                sprintf(outbuf, "SSE2");
        else if ( has_sse() )
                sprintf(outbuf, "SSE");
        else
                *outbuf = '\0';
           
#endif
}

void cpu_brand_string( char* s )
{
    int cpu_info[4] = { 0 };
    int num_ext_ids;
    cpuid( VENDOR_ID, cpu_info );
    if ( cpu_info[ EAX_Reg ] >= 4 )
    {
        cpuid( PROCESSOR_BRAND_STRING_1, cpu_info );
        memcpy( s, cpu_info, sizeof(cpu_info) );
        cpuid( PROCESSOR_BRAND_STRING_2, cpu_info );
        memcpy( s + 16, cpu_info, sizeof(cpu_info) );
        cpuid( PROCESSOR_BRAND_STRING_3, cpu_info );
        memcpy( s + 32, cpu_info, sizeof(cpu_info) );
    }
}    




