/*
Zlib License
--------------------------------------------------
Copyright (c) 2021 migf1@hotmail.com

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
--------------------------------------------------
*/
/**
 * @file	msutimer.h
 * @version	0.01 alpha
 * @date	19 June, 2021
 * @author 	migf1 <mig_f1@hotmail.com>
 * @par Language:
 *		ISO C99
 * @par Online Docs:
 *		https://migf1.github.io/msutimer-docs/
 * @par License
 *		This project is released under the zlib license (see LICENSE.txt)
 *
 * @brief Simple High Resolution Timer for C (public interface).
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
