/* ==================================
 * Private Implementation Module
 * ==================================
 */
// Debug build (no exit on errors):
// gcc -std=c99 -g3 -DMSDEBUG -Wall -Wextra -c msutimer.c -o msutimerdbg.o
//
// Debug build (exit on errors):
// gcc -std=c99 -g3 -DMSDEBUG=2 -Wall -Wextra -c msutimer.c -o msutimerdbgX.o
//
// Release build:
// gcc -std=c99 -O2 --Wall -Wextra -c msutimer.c -o msutimer.o

/*
 Refs:
 http://www.songho.ca/misc/timer/timer.html
 https://stackoverflow.com/a/37920181/734848
 https://stackoverflow.com/a/31335254/734848
 https://stackoverflow.com/questions/5248915/execution-time-of-c-program/5249028
 */

#include "msutimer.h"

#include <stdio.h>		// for debug messages
#include <stdlib.h>		// dynamic mem, qsort(), etc

#include <string.h>		// memset(), etc
#include <stdbool.h>	// C99: bool, true, false
#include <time.h>		// clock(), CLOCKS_PER_SEC, etc
#include <float.h>		// DBL_MAX, etc
#include <math.h>		// fabs(), etc
#include <errno.h>

// attempt to auto-detect platform
#if defined(__WIN32)
	#define MSUT_OS_WINDOWS 1
#elif defined(__unix__) || defined (__linux__) || (defined(__APPLE__) && defined(__MACH__))
	#define MSUT_OS_POSIX 1
#else
	// #define MSUT_OS_FALLBACK 1
#endif

// uncomment & adjust to force a platform manually
// #define MSUT_OS_WINDOWS 0
// #define MSUT_OS_POSIX 0

#if MSUT_OS_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>	// for LARGE_INTEGER, QueryPerformanceCounter(), QueryPerformanceFrequency()
	typedef LARGE_INTEGER MSUTimerTime;		// https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-large_integer-r1

#elif MSUT_OS_POSIX
	#include <sys/time.h>	// for struct timeval, gettimeofday(), etc
	typedef struct timeval MSUTimerTime;	// { long int tv_sec, long int tv_usec}

#else	// fallback
	// NOTE: clock() may wraparound as often as every 35 mins (implementation defined)
	typedef clock_t MSUTimerTime;			// typically a long int
#endif

typedef struct MSUTimer_ {
	MSUTimerTime freq;	// ticks per sec
	MSUTimerTime t1;	// ticks of starting time
	double diffusecs;
} MSUTimer;

/* same debugging compiler flag with MyStr */
#if MSUTDEBUG == 1 || MSDEBUG == 1
	#define MSUT_DBGMSG( msgtype, format, ... )\
		fprintf(stderr, "*** MSUTimer %s [ %s: %s():%ld ]\n==> " format, msgtype, __FILE__, __func__, (long)(__LINE__), __VA_ARGS__ )

#elif MSUTDEBUG > 1 || MSDEBUG > 1
	#define MSUT_DBGMSG( msgtype, format, ... )\
		do {\
			fprintf(stderr, "*** MSUTimer %s [ %s: %s():%ld ]\n==> Reporting & exiting...\n==> " format, msgtype, __FILE__, __func__, (long)(__LINE__), __VA_ARGS__ );\
			exit( EXIT_FAILURE );\
		}while(0)
#else
	#define MSUT_DBGMSG( msgtype, format, ... )
#endif

/* ----------------------------------
 * Private Helper Functions
 * ----------------------------------
 */

// ----------------------------------------
//
static inline bool get_msuttime_( MSUTimerTime *t )
{
#if MSUT_OS_WINDOWS
	return 0 != QueryPerformanceCounter( t );

#elif MSUT_OS_POSIX
	return -1 != gettimeofday( t, NULL );

#else	// fallback
	*t = clock();
	return (clock_t)(-1) != *t;
#endif
}

// ----------------------------------------
//
static inline double msuttime_to_usecs_( const MSUTimerTime *t, const MSUTimerTime *freq )
{
#if MSUT_OS_WINDOWS
	return (double) ((t->QuadPart) * 1000000.0) / freq->QuadPart;

#elif MSUT_OS_POSIX
	(void)freq;
	return (double) (t->tv_sec * 1000000.0) + t->tv_usec;

#else
	(void)freq;
	return ((double)(*t) * 1000000.0) / CLOCKS_PER_SEC;

#endif
}

// ----------------------------------------
//
static inline void update_diffusecs_( MSUTimer *timer, const MSUTimerTime *t2 )
{
#if MSUT_OS_WINDOWS
	// https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
	timer->diffusecs = ((t2->QuadPart - timer->t1.QuadPart) * 1000000.0) / timer->freq.QuadPart;

#elif MSUT_OS_POSIX
	timer->diffusecs = ((t2->tv_sec * 1000000.0) + t2->tv_usec)
		- ((timer->t1.tv_sec * 1000000.0) + timer->t1.tv_usec);

#else	// fallback
	timer->diffusecs = ((double)(*t2 - timer->t1) * 1000000.0) / CLOCKS_PER_SEC;

#endif
}

// ----------------------------------------
// callback for qsort()
static int compare_doubles_for_qsort_( const void *a, const void *b )
{
	const double *da = (const double *) a;
	const double *db = (const double *) b;

	return (*da > *db) - (*da < *db);
}

/* ----------------------------------
 * Public Interface Functions
 * ----------------------------------
 */

// ----------------------------------------
// MSUTimer *msutimer_new( void );
/**
 * Constructs, initializes and starts a new timer. De-allocation should be done
 * by the caller, with msutimer_free().
 *
 * @return
 *		The newly allocated timer, or `NULL` on error.
 *
 * @par Failures:
 * 		- memory allocation failure (`errno` is set by the C runtime)
 * 		- no OS/hardware support for hires timer (`errno` is set to `ERANGE`)
 *
 * @par Sample Usage
 * @code
 		MSUTimer *timer = msutimer_new();
 		if ( !timer ) { handle error here }
 		...
 * @endcode
 */
MSUTimer *msutimer_new( void )
{
	errno = 0;

	MSUTimer *timer = calloc( 1, sizeof(*timer) );
	if ( !timer ) {
		MSUT_DBGMSG( "ERROR", "calloc(%zu) failed. Return: NULL\n", sizeof(*timer) );
		return NULL;
	}

	// Regardless the platform, the following if-checks need to be done just once
	// (hence in the constructor only).

	// reset the created timer

#if MSUT_OS_WINDOWS
	// get ticks per second (frequency)
	if ( 0 == QueryPerformanceFrequency( &timer->freq ) ) {
		// hardware does not support a high-resolution performance counter
		MSUT_DBGMSG( "ERROR", "%s\n", "(ERANGE) QueryPerformanceFrequency() failed. Return: NULL" );
		goto fail;
	}
#endif

	// store current time in timer->t1 as microsecs
	if ( !get_msuttime_( &timer->t1 ) ) {
		// OS does not support getting time zone information, and tzp is not a null pointer.
		MSUT_DBGMSG( "ERROR", "%s\n", "(ERANGE) get_msuttime() failed. Return: NULL" );
		goto fail;
	}

	timer->diffusecs = 0.0;
	return timer;

fail:
	free( timer );
	errno = ERANGE;
	return NULL;
}

// ----------------------------------------
// MSUTimer *msutimer_free( MSUTimer *timer );
/**
 * De-allocates the memory reserved for its timer argument.
 *
 * @param timer
 *		The timer to be freed.
 * @return
 *		Always`NULL`, so the caller can opt to assign it back to the freed pointer,
 *		to avoid leaving it in a dangling state.
 *
 * @par Sample Usage
 * @code
 		MSUTimer *timer1 = msutimer_new();	// if ( !timer1 ) { handle error here }
 		MSUTimer *timer2 = msutimer_new();	// if ( !timer2 ) { handle error here }
 		...
 		msutimer_free( timer2 );			// free timer2 and leave it dangling
 		timer1 = msutimer_free( timer1 );	// free timer1 and reset it to NULL
 * @endcode
 */
MSUTimer *msutimer_free( MSUTimer *timer )
{
	if ( timer ) {
		free( timer );
	}
	return NULL;
}
// ----------------------------------------
// double msutimer_gettime( MSUTimer *timer );
/**
 * Gets the current time and Updates its timer argument with the elapsed time
 * since the previous call of the function, or the construction of the timer
 * (whichever came last). The time-difference can then get fetched as a `double`
 * with one of the functions: msutimer_diff_usecs(), msutimer_diff_msecs(),
 * msutimer_diff_secs(), which return *microseconds*, *millseconds* or *seconds*
 * respectively.
 *
 * Alternatively, the caller may opt to calculate the elapsed time manually, by
 * storing and then subtracting the `double` values returned by 2 calls of the
 * function (similar to how the standard C function `clock()` is used). This
 * also allows for time measurements over multiple calls of the function.
 * The result of the subtraction represents *microseconds*, which can then get
 * passed to the convenience macros MSUT_US2MS() or MSUT_US2S() for converting
 * them to *milliseconds* or *seconds*, respectively.
 *
 * @param timer
 *		The timer to be updated.
 * @return
 *		A `double`, representing the current time in *microseconds*. Useful for
 *		calculating manually the time-difference between 2 calls of the function.
 * @remarks
 *		Typically, the piece of code to be timed is placed between 2 calls of
 *		this function. The very first time, the code may also be placed between
 *		msutimer_new() and this function, but then the manual calculation method
 *		cannot be used (moreover, the construction validation overhead is added
 *		to the result).
 * @par Failures:
 * 		- timer is `NULL` (`errno` is set to `EDOM`)
 * @sa
 *		msutimer_bench(), msutimer_bench_average(), msutimer_bench_median()
 *
 * @par Sample Usage
 *		See [Usage Quick Guide](@ref msut_use) for detailed examples.
 */
double msutimer_gettime( MSUTimer *timer )
{
	errno = 0;
	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "(EDOM) function parameter (timer=NULL). Return: -DBL_MAX" );
		return -DBL_MAX;
	}

	MSUTimerTime t2;
	get_msuttime_( &t2 );
	update_diffusecs_( timer, &t2 );

	timer->t1 = t2;
	return msuttime_to_usecs_( &timer->t1, &timer->freq );
;
}

// ----------------------------------------
// double msutimer_diff_usecs( MSUTimer *timer );
/**
 * Queries its timer argument for the updated time-difference, after a call to
 * msutimer_gettime(). Calling this function before calling msutimer_gettime()
 * first, results to a random value (bogus time-difference).
 *
 * @param timer
 *		The timer to be queried.
 * @return
 *		A `double` reflecting the updated time-difference in *microseconds*
 *		after a call to msutimer_gettime(),	or `-DBL_MAX` on error.
 * @par Failures:
 * 		- timer is `NULL` (`errno` is set to `EDOM`)
 * @sa
 *		msutimer_diff_msecs(), msutimer_diff_secs(), [Usage Quick Guide](@ref msut_use)
 *
 * @par Sample Usage
 * @code
 		MSUTimer *timer = msutimer_new();	// if ( !timer1 ) { handle error here }
 		...
		msutimer_gettime( timer );
		printf( "Elapsed: %.9lf usecs\n", msutimer_diff_usecs(timer) );
		...
		msutimer_gettime( timer );
		printf( "Elapsed: %.9lf usecs\n", msutimer_diff_usecs(timer) );
 		msutimer_free( timer );
 * @endcode
 */
double msutimer_diff_usecs( MSUTimer *timer )
{
	errno = 0;
	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "(EDOM) function parameter (timer=NULL). Return: -DBL_MAX" );
		return -DBL_MAX;
	}
	return timer->diffusecs;
}

// ----------------------------------------
// double msutimer_diff_msecs(MSUTimer *timer);
/**
 * Queries its timer argument for the updated time-difference, after a call to
 * msutimer_gettime(). Calling this function before calling msutimer_gettime()
 * first, results to a random value (bogus time-difference).
 *
 * @param timer
 *		The timer to be queried.
 * @return
 *		A `double` reflecting the updated time-difference in *milliseconds*
 *		after a call to msutimer_gettime(), or `-DBL_MAX` on error.
 * @par Failures:
 * 		- timer is `NULL` (`errno` is set to `EDOM`)
 * @sa
 *		msutimer_diff_usecs(), msutimer_diff_secs(), [Usage Quick Guide](@ref msut_use)
 */
double msutimer_diff_msecs( MSUTimer *timer )
{
	errno = 0;
	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "(EDOM) function parameter (timer=NULL). Return: -DBL_MAX" );
		return -DBL_MAX;
	}
	return 0.001 * timer->diffusecs;
}

// ----------------------------------------
// double msutimer_diff_secs( MSUTimer *timer );
/**
 * Queries its timer argument for the updated time-difference, after a call to
 * msutimer_gettime(). Calling this function before calling msutimer_gettime()
 * first, results to a random value (bogus time-difference).
 *
 * @param timer
 *		The timer to be queried.
 * @return
 *		A `double` reflecting the updated time-difference in *seconds* after
 *		a call to msutimer_gettime(), or `-DBL_MAX` on error.
 * @par Failures:
 * 		- timer is `NULL` (`errno` is set to `EDOM`)
 * @sa
 *		msutimer_diff_usecs(), msutimer_diff_msecs(), [Usage Quick Guide](@ref msut_use)
 */
double msutimer_diff_secs( MSUTimer *timer )
{
	errno = 0;
	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "(EDOM) function parameter (timer=NULL). Return: -DBL_MAX" );
		return -DBL_MAX;
	}
	return 0.000001 * timer->diffusecs;
}

// ----------------------------------------
// double msutimer_accuracy_usecs( MSUTimer *timer )
/**
 * Obtains the minimum time-interval that can be measured by *MSUTimer*, on
 * the running platform. The returned *microseconds* can be converted to
 * *milliseconds* or *seconds*, with the convenience	macros MSUT_US2MS() or
 * MSUT_US2S(), respectively.
 *
 * @param timer
 *		Any already created timer.
 * @return
 *		A `double` representing the *MSUTimer* accuracy on the current platform
 *		in *microseconds*, or `-DBL_MAX` on error.
 * @remarks
 *		On Windows, a typical accuracy is around 0.320 *microseconds*.
 *		Occasionally the function may return a bogus value, but it is pretty
 *		rare (and I really don't know why is that).
 *
 * @par Failures:
 * 		- timer is `NULL` (`errno` is set to `EDOM`)
 * @sa
 *		[Platform Dependent Measurements](@ref msut_pdm)
 *
 * @par Sample Usage
 * @code
		#include "msutimer.h"
		...
		int main( void ) {
			MSUTimer *timer = msutimer_new();
			if (!timer) { fputs("msutimer_new()failed!\n", stderr); exit(1); }

			double usecs = msutimer_accuracy_usecs(timer);
			printf( "MSUTimer accuracy: %.9.lf secs\n", MSUT_US2S(usecs) );
			printf( "MSUTimer accuracy: %.9.lf msecs\n", MSUT_US2MS(usecs) );
			printf( "MSUTimer accuracy: %.9.lf usecs\n", usecs );

			msutimer_free( timer );
			exit(0);
		}
		// Output -------------
		// MSUTimer accuracy: 0.000000320 secs
		// MSUTimer accuracy: 0.000320801 msecs
		// MSUTimer accuracy: 0.320312500 usecs
 * @endcode
 */
// ----------------------------------------
// DUNNO WHY, but with MingW32 it sometimes gives bogus result, especially
// with gettimeofday() and clock(). IS NOT tested with other compilers.
//
double msutimer_accuracy_usecs( MSUTimer *timer )
{
	errno = 0;
	if ( !timer ) {
		MSUT_DBGMSG( "ERROR", "%s\n", "(EDOM) function parameter (timer=NULL). Return: -DBL_MAX" );
		return -DBL_MAX;
	}

	double t1 = msutimer_gettime( timer );
	double t2 = t1;
	for ( ;; ) {
		if ( (t2 = msutimer_gettime(timer)) > t1 ) {
			break;
		}
	}
	return t2 - t1;
}

// ----------------------------------------
// double msutimer_bench( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat );
/**
 * Measures the execution time of its callback-function argument, after a
 * specified number of iterations. 
 *
 * @param timer
 *		An already created timer.
 * @param nrepeats
 *		The number of times to execute the callback-function (iterations).
 * @param callback
 *		The callback-function to be measured. It should be of type `bool` (C99)
 *		returning `false` on error, `true` otherwise. Its prototype should be
 *		the following:
 * @code
		bool (*callback)(void *userdata);
 * @endcode
 * @param userdata
 *		A `void` pointer to caller-defined data, used by the callback-function.
 *		This pointer gets directly passed to the callback-function without any
 *		processing. It may be `NULL` if the callback-functions doesn't use any
 *		extra data.
 * @param erepeat
 *		If non-`NULL`, then in case of callback error (`false`) `erepeat` passes
 *		back to the caller the last successful iteration.
 * @return
 *		A `double` representing the time spent (in *microseconds*) to execute
 *		the callback-function `nrepeats` times.
 *
 *		If the callback-function errors before completing `nrepeats` iterations,
 *		then the returned value is	**negative** and `erepeat` (if non-`NULL`)
 *		passes back to the caller the last successful iteration. Hence a negative
 *		return value reflects the number of *microseconds* elpased until the
 *		`erepeat`'th iteration.
 *
 *		On all other errors, `errno` is set to `EDOM` and the function returns 0.0
 *
 * @remarks
 * 		- When the function returns 0.0, the caller can tell if there was an error
 * 		  by checking `errno` against `EDOM` (or not being 0). In *MSUTimer* debug
 * 		  mode (e.g. compiled with `-DMSUTDEBUG` or `-DMSUTDEBUG=2`) errors are
 * 		  auto reported to `stderr`.
 * 		- The return value can be converted to *milliseconds* or *seconds* with
 * 		  the convenience macros MSUT_US2MS() or MSUT_US2S(), respectively.
 * 		- For an example (without error-handling) see the section
 * 		  [Benchmarking](@ref msut_bench).
 *
 * @par Failures:
 * 		- timer is `NULL` (returns 0.0, `errno` is set to `EDOM`)
 * 		- nrepeats is 0 (returns 0.0, `errno` is set to `EDOM`)
 * 		- callback is `NULL` (returns 0.0, `errno` is set to `EDOM`)
 * 		- callback returns `false` (`erepeat` is set to the last successful
 * 	      iteration and the function returns the elapsed *microseconds* until
 *		  then, as a negative `double`).
 * @sa
 *		msutimer_bench_average(), msutimer_bench_median(), [Benchmarking](@ref msut_bench)
 */
double msutimer_bench( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat )
{
	errno = 0;

	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to timer=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( !callback ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to callback=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( nrepeats == 0 ) {
		errno = EDOM;
		MSUT_DBGMSG( "WARNING", "%s\n", "Immediate return due to nrepeats=0. Returned 0 secs." );
		return 0.0;
	}

	if ( erepeat ) {
		*erepeat = 0;	// reset
	}

	double bias = 1.0;
	double t = msutimer_gettime( timer );
	for (size_t i=0; i < nrepeats; i++) {
		if ( !callback( userdata ) ) {
			if ( erepeat ) {
				*erepeat = i;
			}
			bias = -1.0;
			MSUT_DBGMSG(
				"WARNING",
				"Requested iterations: %zu, but FAILED at %zu (erepeat).\n"
				"==> Returned: elapsed secs until then (with negative sign).\n",
				nrepeats,
				i);
			break;
		}
	}

	return bias * (msutimer_gettime(timer) - t);
}

// ----------------------------------------
// double msutimer_bench_average( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat );
/**
 * Measures the average execution time of its callback-function argument, after
 * a specified number of iterations. 
 *
 * @remarks
 *		For details see the function msutimer_bench(). The only difference
 *		is that msutimer_bench_average() returns the average time after `nrepeats`
 *		iterations, instead of summing up all iterations.
 *
 *		Put otherwise, msutimer_bench() returns the total time after `nrepeats`
 *		iterations, while msutimer_bench_average() returns the average time of
 *		a single call after `nrepeats` iterations.
 *
 *		Compared to msutimer_bench_median(), this function accounts excessive
 *		spikes in the computation ([average vs. median](https://www.diffen.com/difference/Mean_vs_Median)).
 *
 * @sa
 *		msutimer_bench(), msutimer_bench_median(), [Benchmarking](@ref msut_bench)
 */
double msutimer_bench_average( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat )
{
	errno = 0;

	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to timer=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( !callback ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to callback=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( nrepeats == 0 ) {
		errno = EDOM;
		MSUT_DBGMSG( "WARNING", "%s\n", "Immediate return due to nrepeats=0. Returned 0 secs." );
		return 0.0;
	}

	// run callback nrepeats times and time the average
	double bias = 1.0;
	double sum = 0;
	double t;
	for (size_t i=0; i < nrepeats; i++) {
		t = msutimer_gettime( timer );

		if ( !callback( userdata ) ) {
			if ( erepeat ) {
				*erepeat = i;
			}
			bias = -1.0;
			MSUT_DBGMSG(
				"WARNING",
				"Requested iterations: %zu, but FAILED at %zu (erepeat).\n"
				"==> Returned: average secs until then (with negative sign).\n",
				nrepeats,
				i+1
				);
			nrepeats = i;	// # of successful iterations before failure
			break;
		}

		t = msutimer_gettime( timer ) - t;
		sum += t;
	}

	return bias * sum / nrepeats;
}

// ----------------------------------------
// double msutimer_bench_median( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat );
/**
 * Measures the median execution time of its callback-function argument, after
 * a specified number of iterations. 
 *
 * @remarks
 *		For details see the function msutimer_bench(). The only difference
 *		is that msutimer_bench_median() returns the median time after `nrepeats`
 *		iterations, instead of summing up all iterations.
 *
 *		Put otherwise, msutimer_bench() returns the total time after `nrepeats`
 *		iterations, while msutimer_bench_median() returns the median time of
 *		a single call after `nrepeats` iterations.
 *
 *		Compared to msutimer_bench_average(), this function ignores excessive
 *		spikes in the computation ([average vs. median](https://www.diffen.com/difference/Mean_vs_Median)).
 *
 * @sa
 *		msutimer_bench(), msutimer_bench_median(), [Benchmarking](@ref msut_bench)
 */
double msutimer_bench_median( MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat )
{
	errno = 0;

	if ( !timer ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to timer=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( !callback ) {
		errno = EDOM;
		MSUT_DBGMSG( "ERROR", "%s\n", "Immediate return due to callback=NULL. Return: 0 secs." );
		return 0.0;
	}
	if ( nrepeats == 0 ) {
		errno = EDOM;
		MSUT_DBGMSG( "WARNING", "%s\n", "Immediate return due to nrepeats=0. Returned 0 secs." );
		return 0.0;
	}

	// array of recorded times
	double *rectimes = malloc( nrepeats * sizeof(double) );
	if ( !rectimes ) {
		MSUT_DBGMSG( "ERROR", "malloc(%zu) failed! Returned: 0 secs.\n", nrepeats * sizeof(double) );
		return 0.0;
	}

	// record nrepeats timings
	double bias = 1.0;
	double t;
	for (size_t i=0; i < nrepeats; i++) {
		t = msutimer_gettime( timer );

		if ( !callback( userdata ) ) {
			if ( erepeat ) {
				*erepeat = i;
			}
			bias = -1.0;
			MSUT_DBGMSG(
				"WARNING",
				"Requested iterations: %zu, but FAILED at %zu (erepeat).\n"
				"==> Returned: median secs until then (with negative sign).\n",
				nrepeats,
				i+1
				);
			nrepeats = i;	// # of successful iterations before failure
			break;
		}
		rectimes[i] = msutimer_gettime(timer) - t;
	}

	// get median time (middle elem in sorted array)
	qsort( rectimes, nrepeats, sizeof(double), compare_doubles_for_qsort_ );
	double ret = rectimes[ nrepeats/2 ];	// if even # of elems, get average of the middle two
	if ( nrepeats % 2 == 0 ) {
		ret = (ret + rectimes[1 + nrepeats/2]) / 2;
	}

	free( rectimes );
	return bias * ret;
}
