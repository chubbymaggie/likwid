/*
 * =======================================================================================
 *
 *      Filename:  timer.c
 *
 *      Description:  Implementation of timer module
 *
 *      Version:   <VERSION>
 *      Released:  <DATE>
 *
 *      Author:  Jan Treibig (jt), jan.treibig@gmail.com
 *      Project:  likwid
 *
 *      Copyright (C) 2015 RRZE, University Erlangen-Nuremberg
 *
 *      This program is free software: you can redistribute it and/or modify it under
 *      the terms of the GNU General Public License as published by the Free Software
 *      Foundation, either version 3 of the License, or (at your option) any later
 *      version.
 *
 *      This program is distributed in the hope that it will be useful, but WITHOUT ANY
 *      WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 *      PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along with
 *      this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * =======================================================================================
 */
/* #####   HEADER FILE INCLUDES   ######################################### */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <types.h>
#include <error.h>
#include <likwid.h>
/* #####   EXPORTED VARIABLES   ########################################### */
/* #####   VARIABLES  -  LOCAL TO THIS SOURCE FILE   ###################### */
static uint64_t baseline = 0ULL;
static uint64_t cpuClock = 0ULL;
static uint64_t sleepbase = 0ULL;
static int timer_initialized = 0;

void (*TSTART)(TscCounter*) = NULL;
void (*TSTOP)(TscCounter*) = NULL;

/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ######################### */
#define CPUID                              \
    __asm__ volatile ("cpuid"                             \
            : "=a" (eax),     \
            "=b" (ebx),     \
            "=c" (ecx),     \
            "=d" (edx)      \
            : "0" (eax), "2" (ecx))

/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ########### */
#ifdef __x86_64
static void fRDTSC(TscCounter* cpu_c)
{
    __asm__ volatile("xor %%eax,%%eax\n\t"           \
    "cpuid\n\t"           \
    "rdtsc\n\t"           \
    "movl %%eax, %0\n\t"  \
    "movl %%edx, %1\n\t"  \
    : "=r" ((cpu_c)->int32.lo), "=r" ((cpu_c)->int32.hi) \
    : : "%eax","%ebx","%ecx","%edx");
}

static void fRDTSC_CR(TscCounter* cpu_c)
{
    __asm__ volatile(   \
    "rdtsc\n\t"           \
    "movl %%eax, %0\n\t"  \
    "movl %%edx, %1\n\t"  \
    : "=r" ((cpu_c)->int32.lo), "=r" ((cpu_c)->int32.hi) \
    : : "%eax","%ebx","%ecx","%edx");
}
#ifndef __MIC__
static void fRDTSCP(TscCounter* cpu_c)
{
    __asm__ volatile(     \
    "rdtscp\n\t"          \
    "movl %%eax, %0\n\t"  \
    "movl %%edx, %1\n\t"  \
    "cpuid\n\t"           \
    : "=r" ((cpu_c)->int32.lo), "=r" ((cpu_c)->int32.hi) \
    : : "%eax","%ebx","%ecx","%edx");
}
#endif
#endif

static void _timer_start( TimerData* time )
{
#ifdef __x86_64
    if (TSTART)
        TSTART(&(time->start));
#endif
#ifdef _ARCH_PPC
    uint32_t tbl, tbu0, tbu1;

    do {
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
        __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
    } while (tbu0 != tbu1);

    time->start.int64 = (((uint64_t)tbu0) << 32) | tbl;
#endif
}

static void _timer_stop( TimerData* time )
{
#ifdef __x86_64
    if (TSTOP)
        TSTOP(&(time->stop));
#endif
#ifdef _ARCH_PPC
    uint32_t tbl, tbu0, tbu1;
    do {
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
        __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
    } while (tbu0 != tbu1);

    time->stop.int64 = (((uint64_t)tbu0) << 32) | tbl;
#endif
}

static uint64_t _timer_printCycles( TimerData* time )
{
    /* clamp to zero if something goes wrong */
    if (((time->stop.int64-baseline) < time->start.int64) ||
        (time->start.int64 == time->stop.int64))
    {
        return 0ULL;
    }
    else
    {
        return (time->stop.int64 - time->start.int64 - baseline);
    }
}

/* Return time duration in seconds */
static double _timer_print( TimerData* time )
{
    uint64_t cycles;
    /* clamp to zero if something goes wrong */
    if (((time->stop.int64-baseline) < time->start.int64) ||
        (time->start.int64 == time->stop.int64))
    {
        cycles = 0ULL;
    }
    else
    {
        cycles = time->stop.int64 - time->start.int64 - baseline;
    }
    return  ((double) cycles / (double) cpuClock);
}

static uint64_t
getCpuSpeed(void)
{
#ifdef __x86_64
    int i;
    TimerData data;
    TscCounter start;
    TscCounter stop;
    uint64_t result = 0xFFFFFFFFFFFFFFFFULL;
    struct timeval tv1;
    struct timeval tv2;
    struct timezone tzp;
    struct timespec delay = { 0, 500000000 }; /* calibration time: 500 ms */

    for (i=0; i< 10; i++)
    {
        _timer_start(&data);
        _timer_stop(&data);
        result = MIN(result,_timer_printCycles(&data));
    }

    baseline = result;
    result = 0xFFFFFFFFFFFFFFFFULL;
    data.stop.int64 = 0;
    data.start.int64 = 0;

    for (i=0; i< 2; i++)
    {
        _timer_start(&data);
        gettimeofday( &tv1, &tzp);
        nanosleep( &delay, NULL);
        _timer_stop(&data);
        gettimeofday( &tv2, &tzp);

        result = MIN(result,(data.stop.int64 - data.start.int64));
    }

    return (result) * 1000000 /
        (((uint64_t)tv2.tv_sec * 1000000 + tv2.tv_usec) -
         ((uint64_t)tv1.tv_sec * 1000000 + tv1.tv_usec));
#endif
#ifdef _ARCH_PPC
    FILE *fpipe;
    char *command="grep timebase /proc/cpuinfo | awk '{ print $3 }'";
    char buff[256];

    if ( !(fpipe = (FILE*)popen(command,"r")) )
    {  // If fpipe is NULL
        perror("Problems with pipe");
        exit(1);
    }

    fgets(buff, 256, fpipe);

    return (uint64_t)   atoi(buff);
#endif
}



/* #####   FUNCTION DEFINITIONS  -  EXPORTED FUNCTIONS   ################## */

void init_sleep()
{
    int status;
    TimerData timer;
    struct timespec req = {0,1};
    struct timespec rem = {0,0};
    for (int i=0; i<10; ++i)
    {
        _timer_start(&timer);
        status = clock_nanosleep(CLOCK_REALTIME,0,&req, &rem);
        _timer_stop(&timer);
        if (_timer_print(&timer)*1E6 > sleepbase)
        {
            sleepbase = _timer_print(&timer)*1E6 + 2;
        }
    }
}


void timer_init( void )
{
    uint32_t eax,ebx,ecx,edx;
    if (timer_initialized == 1)
    {
        return;
    }
    if ((!TSTART) && (!TSTOP))
    {
        TSTART = fRDTSC;
        eax = 0x80000001;
        CPUID;
#ifndef __MIC__
        if (edx & (1<<27))
        {
            TSTOP = fRDTSCP;
        }
        else
        {
            TSTOP = fRDTSC_CR;
        }
#else
        TSTOP = fRDTSC_CR;
#endif
    }
    if (cpuClock == 0ULL)
    {
        cpuClock = getCpuSpeed();
    }
    timer_initialized = 1;
}

uint64_t timer_printCycles( TimerData* time )
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return 0ULL;
    }
    return _timer_printCycles(time);
}

/* Return time duration in seconds */
double timer_print( TimerData* time )
{
    uint64_t cycles;
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return 0ULL;
    }
    return _timer_print(time);
}

uint64_t timer_getCpuClock( void )
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return 0ULL;
    }
    return cpuClock;
}

uint64_t timer_getBaseline( void )
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return 0ULL;
    }
    return baseline;
}

void timer_start( TimerData* time )
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return;
    }
    _timer_start(time);
}


void timer_stop( TimerData* time )
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return;
    }
    _timer_stop(time);
}



int timer_sleep(unsigned long usec)
{
    int status = -1;
    struct timespec req;
    struct timespec rem = {0,0};
    if (sleepbase == 0x0ULL)
    {
        init_sleep();
    }
    if (usec >= 1000000)
    {
        status = sleep(usec / 1000000);
    }
    else
    {
        req.tv_sec = 0;
        req.tv_nsec = (usec-sleepbase)*1.E3;
        status = clock_nanosleep(CLOCK_REALTIME,0,&req, &rem);
        if ((status == -1) && (errno == EINTR))
        {
            status = (rem.tv_sec * 1E6) + (rem.tv_nsec * 1E-3);
        }
    }
    return status;
}


void timer_finalize(void)
{
    if (timer_initialized != 1)
    {
        ERROR_PLAIN_PRINT(Timer module not properly initialized);
        return;
    }
    baseline = 0ULL;
    cpuClock = 0ULL;
    TSTART = NULL;
    TSTOP = NULL;
    timer_initialized = 0;
}

void timer_reset( TimerData* time )
{
    time->start.int64 = 0;
    time->stop.int64 = 0;
}
