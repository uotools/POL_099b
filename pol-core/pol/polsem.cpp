/*
History
=======


Notes
=======

*/

#include "polsem.h"

#include "checkpnt.h"

#include "../clib/logfacility.h"
#include "../clib/passert.h"
#include "../clib/strexcpt.h"
#include "../clib/threadhelp.h"
#include "../clib/tracebuf.h"

#ifdef _WIN32
#   include <process.h>
#else
#   include <pthread.h>
#   include <sys/time.h>
#   include <unistd.h>
#endif

namespace Pol {
  namespace Core {
#ifdef _WIN32
	DWORD locker;
	void polsem_lock()
	{
	  DWORD tid = GetCurrentThreadId();
	  EnterCriticalSection( &cs );
	  passert_always( locker == 0 );
	  locker = tid;
	}

	void polsem_unlock()
	{
	  DWORD tid = GetCurrentThreadId();
	  passert_always( locker == tid );
	  locker = 0;
	  LeaveCriticalSection( &cs );
	}
#else
	pid_t locker;
	void polsem_lock()
	{
	  pid_t pid = getpid();
	  int res = pthread_mutex_lock( &polsem );
	  if (res != 0 || locker != 0)
	  {
        POLLOG.Format( "pthread_mutex_lock: res={}, pid={}, locker={}\n")<< res<< pid<< locker;
	  }
	  passert_always( res == 0 );
	  passert_always( locker == 0 );
	  locker = pid;
	}
	void polsem_unlock()
	{
	  pid_t pid = getpid();
	  passert_always( locker == pid );
	  locker = 0;
	  int res = pthread_mutex_unlock( &polsem );
	  if (res != 0)
	  {
        POLLOG.Format( "pthread_mutex_unlock: res={},pid={}") << res << pid;
	  }
	  passert_always( res == 0 );
	}

#endif

    
#ifdef _WIN32
	CRITICAL_SECTION cs;
	HANDLE hEvPulse;

	HANDLE hEvTasksThread;
	HANDLE hEvClientTransmit;

	CRITICAL_SECTION csThread;
	HANDLE hSemThread;

	void init_ipc_vars()
	{
	  InitializeCriticalSection( &cs );
	  hEvPulse = CreateEvent( NULL, TRUE, FALSE, NULL );

	  hEvTasksThread = CreateEvent( NULL, FALSE, FALSE, NULL );

	  hEvClientTransmit = CreateEvent( NULL, TRUE, FALSE, NULL );

	  InitializeCriticalSection( &csThread );
	  hSemThread = CreateSemaphore( NULL, 0, 1, NULL );
	}

	void deinit_ipc_vars()
	{
	  CloseHandle( hSemThread );
	  DeleteCriticalSection( &csThread );

	  CloseHandle( hEvTasksThread );
	  hEvTasksThread = NULL;

	  CloseHandle( hEvPulse );
	  CloseHandle( hEvClientTransmit );
	  DeleteCriticalSection( &cs );
	}
	void send_pulse()
	{
      TRACEBUF_ADDELEM( "Pulse", 1 );
	  PulseEvent( hEvPulse );
	}

	void wait_for_pulse( unsigned int millis )
	{
	  WaitForSingleObject( hEvPulse, millis );
	}

	void wake_tasks_thread()
	{
	  SetEvent( hEvTasksThread );
	}

	void tasks_thread_sleep( unsigned int millis )
	{
	  WaitForSingleObject( hEvTasksThread, millis );
	}

	void send_ClientTransmit_pulse()
	{
      TRACEBUF_ADDELEM( "ClientTransmitPulse", 1 );
	  PulseEvent( hEvClientTransmit );
	}

	void wait_for_ClientTransmit_pulse( unsigned int millis )
	{
	  WaitForSingleObject( hEvClientTransmit, millis );
	}
#else

    pthread_mutexattr_t polsem_attr;
	pthread_mutex_t polsem;
	//pthread_mutex_t polsem = PTHREAD_MUTEX_INITIALIZER;
	// pthread_mutex_t polsem = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP; 

	pthread_mutex_t pulse_mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t pulse_cond = PTHREAD_COND_INITIALIZER;

	pthread_mutex_t clienttransmit_pulse_mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t clienttransmit_pulse_cond = PTHREAD_COND_INITIALIZER;

	pthread_mutex_t task_pulse_mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t task_pulse_cond = PTHREAD_COND_INITIALIZER;

	pthread_mutex_t threadstart_mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t threadstart_pulse_mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t threadstart_pulse_cond = PTHREAD_COND_INITIALIZER;
	bool thread_started;


	pthread_mutex_t polsemdbg_mut = PTHREAD_MUTEX_INITIALIZER;

	pthread_attr_t thread_attr;

	void init_ipc_vars()
	{
	  int res;
	  res = pthread_mutexattr_init( &polsem_attr );
	  passert_always( res == 0 );

	  /*
		  res = pthread_mutexattr_setkind_np( &polsem_attr, PTHREAD_MUTEX_ERRORCHECK_NP );
		  passert_always( res == 0 );

		  res = pthread_mutexattr_settype( &polsem_attr, PTHREAD_MUTEX_ERRORCHECK );
		  passert_always( res == 0 );
		  */

	  res = pthread_mutex_init( &polsem, &polsem_attr );
	  passert_always( res == 0 );

	  pthread_attr_init( &thread_attr );
	  pthread_attr_setdetachstate( &thread_attr, PTHREAD_CREATE_DETACHED );
	}

	void deinit_ipc_vars()
	{
	}

	void send_pulse()
	{
	  pthread_mutex_lock( &pulse_mut );
	  pthread_cond_broadcast( &pulse_cond );
	  pthread_mutex_unlock( &pulse_mut );
	}

	void calc_abs_timeout( struct timespec* ptimeout, unsigned int millis )
	{
	  struct timeval now;
	  struct timezone tz;

	  gettimeofday(&now, &tz);
	  int add_sec = 0;
	  if (millis > 1000)
	  {
		add_sec = millis / 1000;
		millis -= (add_sec * 1000);
	  }
	  ptimeout->tv_sec = now.tv_sec + add_sec;

	  ptimeout->tv_nsec = now.tv_usec * 1000 + millis * 1000000L;
	  if (ptimeout->tv_nsec >= 1000000000)
	  {
		++ptimeout->tv_sec;
		ptimeout->tv_nsec -= 1000000000;
	  }
	}

	void wait_for_pulse( unsigned int millis )
	{
	  struct timespec timeout;

	  pthread_mutex_lock(&pulse_mut);

	  calc_abs_timeout( &timeout, millis );

	  pthread_cond_timedwait(&pulse_cond, &pulse_mut, &timeout);

	  pthread_mutex_unlock(&pulse_mut);
	}

	void wake_tasks_thread()
	{
	  pthread_mutex_lock( &task_pulse_mut );
	  pthread_cond_broadcast( &task_pulse_cond );
	  pthread_mutex_unlock( &task_pulse_mut );
	}

	void tasks_thread_sleep( unsigned int millis )
	{
	  struct timespec timeout;

	  pthread_mutex_lock(&task_pulse_mut);

	  calc_abs_timeout( &timeout, millis );

	  pthread_cond_timedwait(&task_pulse_cond, &task_pulse_mut, &timeout);

	  pthread_mutex_unlock(&task_pulse_mut);
	}

	void send_ClientTransmit_pulse()
	{
	  pthread_mutex_lock( &clienttransmit_pulse_mut );
	  pthread_cond_broadcast( &clienttransmit_pulse_cond );
	  pthread_mutex_unlock( &clienttransmit_pulse_mut );
	}

	void wait_for_ClientTransmit_pulse( unsigned int millis )
	{
	  struct timespec timeout;

	  pthread_mutex_lock(&clienttransmit_pulse_mut);

	  calc_abs_timeout( &timeout, millis );

	  pthread_cond_timedwait(&clienttransmit_pulse_cond, &clienttransmit_pulse_mut, &timeout);

	  pthread_mutex_unlock(&clienttransmit_pulse_mut);
	}

#endif
  }
}