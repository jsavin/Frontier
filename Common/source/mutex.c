/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/* Created 15 Aug 2011 Brent Simmons */

#include <pthread.h>
#include "mutex.h"

int mutexCreateRecursive(pthread_mutex_t *lockToCreate) {
	pthread_mutexattr_t lockAttributes;
	int pthreadErrorCode = pthread_mutexattr_init(&lockAttributes);
	if (pthreadErrorCode != 0)
		return pthreadErrorCode;
	pthread_mutexattr_settype(&lockAttributes, PTHREAD_MUTEX_RECURSIVE);
	pthreadErrorCode = pthread_mutex_init(lockToCreate, &lockAttributes);
	pthread_mutexattr_destroy(&lockAttributes);
	return pthreadErrorCode;
}


