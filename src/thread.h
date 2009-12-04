/*********************************************************************
 *  This file is part of LHC
 *
 *  Copyright (c) 2009 Matthias Richter
 * 
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 * 
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef THREAD_H
#define THREAD_H

#ifdef WIN32

#error "Threads are not yet implemented, sorry"

#define lhc_thread ???
#define lhc_mutex ???

#define lhc_thread_create(thread, callback, user_arg) ???
#define lhc_thread_join(thread, retval) ???
#define lhc_thread_cancel(thread) ???
#define lhc_thread_exit(thread) ???

#define lhc_mutex_init(mutex) pthread_mutex_init((mutex), NULL)
#define lhc_mutex_destroy(mutex) pthread_mutex_destroy((mutex))
#define lhc_mutex_lock(mutex) pthread_mutex_lock((mutex))
#define lhc_mutex_trylock(mutex) pthread_mutex_trylock((mutex))
#define lhc_mutex_unlock(mutex) pthread_mutex_unlock((mutex))

#else

#include <pthread.h>

#define lhc_thread pthread_t
#define lhc_mutex pthread_mutex_t

#define lhc_thread_create(thread, callback, user_arg) pthread_create((thread), NULL, (callback), (user_arg))
#define lhc_thread_join(thread, retval) pthread_join((thread), (retval))
#define lhc_thread_cancel(thread) pthread_cancel((thread))
#define lhc_thread_exit(thread) pthread_exit(thread)

#define lhc_mutex_init(mutex) pthread_mutex_init((mutex), NULL)
#define lhc_mutex_destroy(mutex) pthread_mutex_destroy((mutex))
#define lhc_mutex_lock(mutex) pthread_mutex_lock((mutex))
#define lhc_mutex_trylock(mutex) pthread_mutex_trylock((mutex))
#define lhc_mutex_unlock(mutex) pthread_mutex_unlock((mutex))

#endif

#define CRITICAL_SECTION(lock) for (lhc_mutex_lock((lock)); ((lhc_mutex_trylock((lock)) != 0) || (lhc_mutex_unlock((lock)) != 0)); lhc_mutex_unlock((lock)))

#endif /* THREAD_H */
