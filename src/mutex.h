#ifndef _MUTEX_H
#define _MUTEX_H

#if defined(_WIN32) || defined(WINDOWS)

/* untested! */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

#define mutex_t HANDLE
inline int mutex_init(mutex_t* m)
{
	*m = CreateMutex(NULL, FALSE, NULL);
	return (NULL == *m);
}
#define mutex_lock(m)    (WAIT_FAILED == WaitForSingleObject(*(m), INFINITE))
#define mutex_unlock(m)  (0 == ReleaseMutex(*(m)))
#define mutex_destroy(m) (0 == CloseHandle(*(m)))

#else /* assume posix */

#include <pthread.h>

#define mutex_t          pthread_mutex_t
#define mutex_init(m)    pthread_mutex_init((m), NULL)
#define mutex_lock(m)    pthread_mutex_lock((m))
#define mutex_unlock(m)  pthread_mutex_unlock((m))
#define mutex_destroy(m) pthread_mutex_destroy((m));

#endif

#endif
