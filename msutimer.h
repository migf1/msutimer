/**
 * @file	msutimer.h
 * @version	0.01 alpha
 * @date	19 June, 2021
 * @author 	migf1 <mig_f1@hotmail.com>
 * @par Language: ISO C99
 *
 * @brief Simple High Resolution Timer for C.
 *
 * Public interface to *MSUTimer* private implementation.
 *
 */

#ifndef MSUTIMER_H			/* start of inclusion guard */
#define MSUTIMER_H

#include <stddef.h>		// size_t, etc
#include <stdbool.h>	// C99: bool, true, false

/// Opaque type (forward-declaration)
typedef struct MSUTimer_ MSUTimer;

/// @name Convenience Macros
/// Benchmark functions (and others) return *microseconds*, so the following macros
/// may prove handy to the caller.
/// @{
#define MSUT_US2MS(usecs)	((usecs) * 0.001)		///< Convert microseconds to milliseconds.
#define MSUT_US2S(usecs)	((usecs) * 0.000001)	///< Convert microseconds to seconds.
/// @}

MSUTimer *msutimer_new(void);					///< Create a new timer.
MSUTimer *msutimer_free(MSUTimer *timer);		///< Free an existing timer.
double msutimer_accuracy_usecs(MSUTimer *timer);///< Get *MSUTimer* accuracy.
double msutimer_gettime( MSUTimer *timer );		///< Get current time & update time-difference.
double msutimer_diff_usecs(MSUTimer *timer);	///< Get updated time-difference in *microseconds*.
double msutimer_diff_msecs(MSUTimer *timer);	///< Get updated time-difference in *milliseconds*.
double msutimer_diff_secs(MSUTimer *timer);		///< Get updated time-difference in *seconds*.

												/// Get callback's execution time.
double msutimer_bench(MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat);
												/// Get callback's average execution time.
double msutimer_bench_average(MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat);
												/// Get callback's median execution time.
double msutimer_bench_median(MSUTimer *timer, size_t nrepeats, bool (*callback)(void *), void *userdata, size_t *erepeat);

#endif					/* end of inclusion guard */
