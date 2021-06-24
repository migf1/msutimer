Intro
=====

Despite being usually distributed with *MyStr*, *MSUTimer* is stand alone
(zero dependencies). It is just a little compliment on the side, for measuring
small time-intervals.

This is a toy project really, mostly suitable for quick & dirty time measurements,
no more than a few dozens of minutes apart (unless used on Windows). There is no
overflow guarding, no special care for multi thread/processor environments, no
timing consistency across different platforms.

That said, it's by far more useful & reliable on Windows, under which it was
compiled anyway (using MinGW32).

Platform Dependent Measurements
===============================

The function msutimer_accuracy_usecs() may be used to get *MSUTimer*'s accuracy
in *microseconds*, at run time.

~~~~{.c}
double msutimer_accuracy_usecs( MSUTimer *timer );
~~~~

*MSUTimer* works best on **Windows**, due to using directly the native function
`QueryPerformanceCounter()`. This function gives a resolution less than 1
*microsecond*, it is not affected by system time changes and processor speeds,
and it doesn't roll over for at least 100 years since a system boot.

The **POSIX** equivalent is `clock_gettime(CLOCK_MONOTONIC_RAW)`, but to the best
of my knowledge it is still not consistently available across all implementations.
Most importantly, currently I don't have easy access to POSIX, so I just used
`gettimeofday()` instead, which is barely an equivalent. It gets affected by
system time changes, with an implementation defined resolution (usually
*microseconds* but not always), but it is available everywhere.

> Adding `clock_gettime(CLOCK_MONOTONIC_RAW)` when available, under POSIX, should
> be fairly easy by checking the preprocessor directive `_POSIX_TIMERS`. I'll
> probably do it, once I'm able to properly test it.

Same goes for **macOS**, *MSSUTimer* currently also uses `gettimeofday()` when
compiled under that OS.

Lastly,`clockl()` is used as fallback on all other platforms (note that depending
on the implementation, this function can roll over as often as every 35 minutes).

Here is the quick summary of the clocking functions picked at compile time,
depending on the platform:
- on Windows: `QueryPerformanceCounter()`
- on Unix/Linux/macOS: `gettimeofday()`
- fallback: `clock()`

Compilation {#msut_compile}
===========

Compilation is pretty straight forward, just make sure you enable C99 support.

~~~~{.c}
	// For release builds
	gcc -std=c99 -O2 msutimer.c -o msutimer.o
~~~~

There is also an `MSUTDEBUG` preprocessor directive, for debugging purposes.
When defined with a value of 1, it reports errors on `stderr`. When defined with
a value of 2 or greater, it additionally stops the execution after reporting an
error.

~~~~{.c}
	// For debug builds, with reporting but no exiting on errors
	gcc -std=c99 -g3 -DMSUTDEBUG -Wall -Wextra msutimer.c -o msutimerDbg.o

	// For debug builds, with reporting AND exiting on errors
	gcc -std=c99 -g3 -DMSUTDEBUG=2 -Wall -Wextra msutimer.c -o msutimerDbgX.o

~~~~

> `MSDEBUG` may also be used instead of `MSUTDEBUG` (just to stay consistent with
> *MyStr*).

Usage {#msut_use}
=====

To use *MSUTimer* we can either build it as an object or library file and then
link against it, or directly include its files (msutimer.c and msutimer.h) in
our project.

After creating a timer with msutimer_new(), the elapsed time between 2 calls
of msutimer_gettime() gets auto stored into the timer, in *microseconds*. It
can then be retrieved as a `double` with the functions msutimer_diff_secs(),
msutimer_diff_msecs() or msutimer_diff_usecs(). They return *seconds*,
*milliseconds* or *microseconds* respectively.

Alternatively, the returned `double` values of 2 msutimer_gettime() calls can
be stored manually by the caller, and then subtracted in order to obtain the
time-difference.
This is similar to how the standard C function `clock()` is used. The macros
MSUT_US2MS() or MSUT_US2S() may then be used to convert the calculated
difference to *milliseconds* or *seconds*, respectively. This manual approach
also allows for cumulative timings over multiple calls of msutimer_gettime(),
without needing a 2nd timer (see the variable `totusecs` in the code snippet
below).  

When a timer is no longer needed, it should be freed with msutimer_free().

The following code-snippet demonstrates both auto and manual approaches,
along with cumulative timing which is manual only (the `totusecs` variable):

~~~~{.c}
	#include "msutimer.h"
	...
	int main( void )
	{
		MSUTimer *timer = msutimer_new();
		if (!timer) { fputs("msutimer_new() failed!\n", stderr); exit(1); }

		double totusecs = msutimer_gettime(timer);		// store overall starting time

		// AUTO TIME-DIFFERENCE & CONVERSIONS
		msutimer_gettime(timer);						// new starting time
		// ... code to be timed goes here ...
		msutimer_gettime(timer);						// stopping time
		printf( "%.9lf secs\n", msutimer_diff_secs(timer) );	// print diff in secs
		printf( "%.9lf msecs\n", msutimer_diff_msecs(timer) );	// print diff in millisecs
		printf( "%.9lf usecs\n", msutimer_diff_usecs(timer) );	// print diff in microsecs

		// MANUAL TIME-DIFFERENCE & CONVERSIONS
		double usecs = msutimer_gettime(timer);			// store another new starting time
		// ... code to be timed goes here ...
		usecs = msutimer_gettime(timer) - usecs;		// calc & store time diff
		printf( "\n%.9lf secs\n", MSUT_US2S(usecs) );	// print diff in secs
		printf( "%.9lf msecs\n", MSUT_US2MS(usecs) );	// print diff in millisecs
		printf( "%.9lf usecs\n", usecs );				// print diff in microsecs

		totusecs = msutimer_gettime(timer) - totusecs;	// calc & store overall time diff
		printf( "\n%.9lf secs\n", MSUT_US2S(totusecs) );// print overall time diff in secs

		msutimer_free( timer );
		exit(0);
	}
~~~~

Benchmarking {#msut_bench}
============

There are also a few benchmarking functions available, namely:
- msutimer_bench()
- msutimer_bench_average()
- msutimer_bench_median()

They all return *microseconds* as a `double`, expecting the piece of code to
be timed as a callback function argument. This caller-defined callback function
should be of type `bool` (C99) returning `false` on error, `true` otherwise
(see the docs of these functions for details). The prototype of the expected
callback function is the following:

~~~~{.c}
	bool (*callback)(void *userdata)
~~~~

Here's a full example, timing 50,000,000 square-root calculations of the same
random number, in 3 different ways:
- running just once and printing the time spent
- printing the average after running 10 times
- printing the median after running 10 times

~~~~{.c}
	#include "msutimer.h"

	#include <math.h>	// for sqrt()
	#include <errno.h>	// for catching sqrt() failures
	#include <stdio.h>	// for fputs(), printf()
	#include <stdlib.h>	// for srand(), rand(), size_t
	#include <time.h>	// for time()

	struct TestData {
		size_t n;	// iterations in callback
		double x;	// num whose square-root will be calc'ed n times
	};
	// ----------------------------------------
	static bool stress_test_callback( void *userdata )
	{
		// no need for extra overhead, unless we don't know who is going to use this func
		// if ( !userdata ) { fputs("callback failed!\n", stderr); return false; }

		struct TestData *td = userdata;
		for (size_t i=0; i < td->n; i++) {
			errno = 0;
			sqrt( td->x );
			if (errno != 0 ) { fputs("sqrt() failed in callback!\n", stderr); return false; }
		}
		return true;
	}
	// ----------------------------------------
	int main( void )
	{
		MSUTimer *timer = msutimer_new();
		if (!timer) { fputs("msutimer_new()failed!\n", stderr); exit(1); }

		srand( time(NULL) );
		struct TestData td = { .n = 50000000, .x = (double)rand() };

		printf( "Timing %zu iterations of sqrt(%.3lf):\n", td.n, td.x );

		size_t ntimes = 1;
		double usecs = msutimer_bench(timer, ntimes, stress_test_callback, &td, NULL);
		printf( "%.9lf secs | Run %zu time(s)\n", MSUT_US2S(usecs), ntimes );

		ntimes = 10;
		usecs = msutimer_bench_average(timer, ntimes, stress_test_callback, &td, NULL);
		printf( "%.9lf secs | Average after %zu times\n", MSUT_US2S(usecs), ntimes );

		usecs = msutimer_bench_median(timer, ntimes, stress_test_callback, &td, NULL);
		printf( "%.9lf secs | Median after %zu times\n", MSUT_US2S(usecs), ntimes );

		msutimer_free( timer );
		exit(0);
	}
~~~~
~~~~{.c}
	// Output -------------
	Timing 50000000 iterations of sqrt(21954.000):
	0.689210245 secs | Run 1 time(s)
	0.713392777 secs | Average after 10 times
	0.714011280 secs | Median after 10 times
~~~~
