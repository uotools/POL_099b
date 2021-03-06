/*
History
=======
2005/12/13 Shinigami: added error code printing in create_thread for debugging
2006/02/06 Shinigami: smaller bugfix in logging
                      error code printing in create_thread extended
2007/02/28 Shinigami: error code printing in create_thread added for linux
2007/03/08 Shinigami: added pthread_exit and _endhreadex to close threads
2008/03/02 Nando: Added bool dec_child to create_thread, used to dec_child_thread_count()
                  if there is an error on create_thread. Will fix some of the zombies.

Notes
=======

*/

#include "threadhelp.h"

#include "passert.h"
#include "logfacility.h"
#include "esignal.h"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4996) // disable warning for strcpy, strerror
#endif

namespace Pol {
  namespace threadhelp {

    ThreadMap threadmap;
    unsigned int child_threads = 0;
    static int threads = 0;

#ifdef _WIN32
    static CRITICAL_SECTION threadhelp_cs;
    static CRITICAL_SECTION threadmap_cs;
    void init_threadhelp()
    {
      InitializeCriticalSection( &threadhelp_cs );
      InitializeCriticalSection( &threadmap_cs );
    }
    void threadsem_lock()
    {
      EnterCriticalSection( &threadhelp_cs );
    }
    void threadsem_unlock()
    {
      LeaveCriticalSection( &threadhelp_cs );
    }

    void threadmap_lock()
    {
      EnterCriticalSection( &threadmap_cs );
    }
    void threadmap_unlock()
    {
      LeaveCriticalSection( &threadmap_cs );
    }

    void thread_sleep_ms( unsigned millis )
    {
      Sleep( millis );
    }
    size_t thread_pid()
    {
      return GetCurrentThreadId();
    }

    const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
      DWORD dwType; // Must be 0x1000.
      LPCSTR szName; // Pointer to name (in user addr space).
      DWORD dwThreadID; // Thread ID (-1=caller thread).
      DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    void _SetThreadName( DWORD dwThreadID, char* name )
    {
      THREADNAME_INFO info;
      info.dwType = 0x1000;
      info.szName = name;
      info.dwThreadID = dwThreadID;
      info.dwFlags = 0;

      __try
      { // oh my god i hate ms ...
        RaiseException( MS_VC_EXCEPTION, 0, sizeof( info ) / sizeof( ULONG_PTR ), (ULONG_PTR*)&info );
      }
      __except ( EXCEPTION_EXECUTE_HANDLER )
      {}
    }
    void SetThreadName( int threadid, std::string threadName )
    {
      char *name = new char[threadName.length() + 1];
      strcpy( name, threadName.c_str() );
      _SetThreadName( threadid, name );
      delete[] name;
    }
#else
    static pthread_mutexattr_t threadsem_attr;
    static pthread_mutex_t threadsem;
    static pid_t threadhelp_locker;

    static pthread_mutexattr_t threadmap_sem_attr;
    static pthread_mutex_t threadmap_sem;

    static pthread_attr_t create_detached_attr;


    void init_threadhelp()
    {
      int res;

      res = pthread_mutexattr_init( &threadsem_attr );
      passert_always( res == 0 );

      res = pthread_mutex_init( &threadsem, &threadsem_attr );
      passert_always( res == 0 );

      res = pthread_mutexattr_init( &threadmap_sem_attr );
      passert_always( res == 0 );

      res = pthread_mutex_init( &threadmap_sem, &threadmap_sem_attr );
      passert_always( res == 0 );

      res = pthread_attr_init( &create_detached_attr );
      passert_always( res == 0 );
      res = pthread_attr_setdetachstate( &create_detached_attr, PTHREAD_CREATE_DETACHED );
      passert_always( res == 0 );
    }
    void threadsem_lock()
    {
      pid_t pid = getpid();
      int res = pthread_mutex_lock( &threadsem );
      if (res != 0)
      {
        POLLOG << "pthread_mutex_lock: res="<< res << ", pid=" << pid << "\n";
      }
      passert_always( res == 0 );
      passert_always( threadhelp_locker == 0 );
      threadhelp_locker = pid;
    }
    void threadsem_unlock()
    {
      pid_t pid = getpid();
      passert_always( threadhelp_locker == pid );
      threadhelp_locker = 0;
      int res = pthread_mutex_unlock( &threadsem );
      if (res != 0)
      {
        POLLOG << "pthread_mutex_unlock: res="<< res << ", pid=" << pid << "\n";
      }
      passert_always( res == 0 );
    }

    void threadmap_lock()
    {
      int res = pthread_mutex_lock( &threadmap_sem );
      if (res != 0)
      {
        POLLOG << "pthread_mutex_lock(threadmap_sem): res="<< res << ", pid=" << getpid() << "\n";
      }
      passert_always( res == 0 );
    }
    void threadmap_unlock()
    {
      int res = pthread_mutex_unlock( &threadmap_sem );
      if (res != 0)
      {
        POLLOG << "pthread_mutex_unlock(threadmap_sem): res="<< res << ", pid=" << getpid() << "\n";
      }
      passert_always( res == 0 );
    }

    void thread_sleep_ms( unsigned millis )
    {
      usleep( millis * 1000L );
    }
    size_t thread_pid()
    {
      return pthread_self();
    }
#endif

    void inc_child_thread_count( bool /*need_lock*/ )
    {
      //if (need_lock)
      threadsem_lock();

      ++child_threads;

      //if (need_lock)
      threadsem_unlock();
    }
    void dec_child_thread_count( bool /*need_lock*/ )
    {
      //if (need_lock)
      threadsem_lock();

      --child_threads;

      //if (need_lock)
      threadsem_unlock();
    }

    void run_thread( void( *threadf )( void ) )
    {
      // thread creator calls inc_child_thread_count before starting thread
      try
      {
        ( *threadf )( );
      }
      catch ( std::exception& ex )
      {
        ERROR_PRINT << "Thread exception: " << ex.what() << "\n";
      }

      dec_child_thread_count();

      threadmap.Unregister( thread_pid() );
    }
    void run_thread( void( *threadf )( void * ), void* arg )
    {
      // thread creator calls inc_child_thread_count before starting thread
      try
      {
        ( *threadf )( arg );
      }
      catch ( std::exception& ex )
      {
        ERROR_PRINT << "Thread exception: " << ex.what() << "\n";
      }

      dec_child_thread_count();

      threadmap.Unregister( thread_pid() );
    }

    class ThreadData
    {
    public:
      std::string name;
      void( *entry )( void* );
      void( *entry_noparam )( void );
      void* arg;
    };

#ifdef _WIN32
    unsigned __stdcall thread_stub2( void *v_td )
#else
    void* thread_stub2( void *v_td)
#endif
    {
      ThreadData* td = reinterpret_cast<ThreadData*>( v_td );

      void( *entry )( void* ) = td->entry;
      void( *entry_noparam )( void ) = td->entry_noparam;
      void* arg = td->arg;

      threadmap.Register( thread_pid(), td->name );

      delete td;
      td = NULL;

      if ( entry != NULL )
        run_thread( entry, arg );
      else
        run_thread( entry_noparam );

#ifdef _WIN32
      _endthreadex( 0 );
#else
      pthread_exit(NULL);
#endif
      return 0;
    }

#ifdef _WIN32
    void create_thread( ThreadData* td, bool dec_child = false )
    {
      unsigned threadid = 0;
      HANDLE h = (HANDLE)_beginthreadex( NULL, 0, thread_stub2, td, 0, &threadid );
      if ( h == 0 ) // added for better debugging
      {
        POLLOG.Format( "error in create_thread: {:d} {:d} \"{:s}\" \"{:s}\" {:d} {:d} {:s} {:d} {:d} {:d}\n" )
          << errno << _doserrno << strerror( errno ) << strerror( _doserrno )
          << threads++ << (unsigned)thread_stub2 << td->name.c_str() << (unsigned)td->entry << (unsigned)td->entry_noparam << td->arg;

        // dec_child says that we should dec_child_threads when there's an error... :)
        if ( dec_child )
          dec_child_thread_count();
      }
      else
      {
        SetThreadName( threadid, td->name );
        CloseHandle( h );
      }
    }
#else
    void create_thread( ThreadData* td, bool dec_child = false )
    {
      pthread_t thread;
      int result = pthread_create( &thread, &create_detached_attr, thread_stub2, td );
      if ( result != 0) // added for better debugging
      {
        POLLOG.Format( "error in create_thread: {:d} {:d} \"{:s}\" {:d} {:} {:s} {:} {:} {:d}\n" )
          << result << errno << strerror( errno )
          << threads++ << reinterpret_cast<const void*>(thread_stub2) << td->name.c_str() << reinterpret_cast<const void*>(td->entry) << reinterpret_cast<const void*>(td->entry_noparam) << td->arg;

        // dec_child says that we should dec_child_threads when there's an error... :)
        if (dec_child)
          dec_child_thread_count();
      }
    }
#endif

    void start_thread( void( *entry )( void* ), const char* thread_name, void* arg )
    {
      auto td = new ThreadData;
      td->name = thread_name;
      td->entry = entry;
      td->entry_noparam = NULL;
      td->arg = arg;

      inc_child_thread_count();

      create_thread( td, true );
    }

    void start_thread( void( *entry )( void ), const char* thread_name )
    {
      auto td = new ThreadData;
      td->name = thread_name;
      td->entry = NULL;
      td->entry_noparam = entry;
      td->arg = NULL;

      inc_child_thread_count();

      create_thread( td, true );
    }
#ifdef _WIN32
    HANDLE ThreadMap::getThreadHandle( size_t pid ) const
    {
      threadmap_lock();
      auto itr = _handles.find( pid );
      if ( itr == _handles.end() )
      {
        threadmap_unlock();
        return 0;
      }
      threadmap_unlock();
      return itr->second;
    }
#endif
    void ThreadMap::Register( size_t pid, const std::string& name )
    {
      threadmap_lock();
      _contents.insert( std::make_pair( pid, name ) );
#ifdef _WIN32
      HANDLE hThread = 0;
      if ( !DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &hThread,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS ) )
      {
        threadmap_unlock();
        ERROR_PRINT << "failed to duplicate thread handle\n";
        return;
      }
      _handles.insert( std::make_pair( pid, hThread ) );
#endif
      threadmap_unlock();
    }
    void ThreadMap::Unregister( size_t pid )
    {
      threadmap_lock();
      _contents.erase( pid );
#ifdef _WIN32
      auto itr = _handles.find( pid );
      if ( itr != _handles.end() )
        CloseHandle( itr->second );
      _handles.erase( pid );
#endif
      threadmap_unlock();
    }
    void ThreadMap::CopyContents( Contents& out ) const
    {
      threadmap_lock();
      out = _contents;
      threadmap_unlock();
    }

    ThreadRegister::ThreadRegister( const std::string &name )
    {
      threadmap.Register( thread_pid(), name );
    }
    ThreadRegister::~ThreadRegister()
    {
      threadmap.Unregister( thread_pid() );
    }


    /// Creates a threadpool of workers.
    /// blocks on deconstruction
    /// eg:
    /// TaskThreadPool workers;
    /// for (....)
    ///   workers.push([&](){dosomework();});
    TaskThreadPool::TaskThreadPool( const std::string& name )
      : _done( false ), _msg_queue()
    {
      // get the count of processors
      unsigned int max_count = std::thread::hardware_concurrency();
      if ( !max_count )  // can fail so at least one
        max_count = 1;
      init( max_count, name );
    }

    TaskThreadPool::TaskThreadPool( unsigned int max_count, const std::string& name )
      : _done( false ), _msg_queue()
    {
      init( max_count, name );
    }

    void TaskThreadPool::init( unsigned int max_count, const std::string& name )
    {
      for ( unsigned int i = 0; i < max_count; ++i )
      {
        _threads.emplace_back( [=]()
        {
          ThreadRegister register_thread( "TaskPool " + name );
          auto f = msg();
          try
          {
            while ( !_done )
            {
              _msg_queue.pop_wait( &f );
              f();
            }
          }
          catch ( msg_queue::Canceled& )
          {
          }
          catch ( std::exception& ex )
          {
            ERROR_PRINT << "Thread exception: " << ex.what() << "\n";
            Clib::force_backtrace( true );
            return;
          }
          //purge the queue empty
          std::list<msg> remaining;
          _msg_queue.pop_remaining( &remaining );
          for ( auto& _f : remaining )
            _f();
        } );
      }
    }

    TaskThreadPool::~TaskThreadPool()
    {
      // send both done and cancel to wake up all workers
      _msg_queue.push( [&]()
      {
        _done = true;
        _msg_queue.cancel();
      } );
      for ( auto& thread : _threads )
        thread.join();
    }

    /// simply fire and forget only the deconstructor ensures the msg to be finished
    void TaskThreadPool::push( msg msg )
    {
      _msg_queue.push( msg );
    }

    /// returns a future which will be set once the msg is processed
    std::future<bool> TaskThreadPool::checked_push( msg msg )
    {
      auto promise = std::make_shared<std::promise<bool>>();
      auto ret = promise->get_future();
      _msg_queue.push( [=]()
      {
        try
        {
          msg();
          promise->set_value( true );
        }
        catch ( ... )
        {
          promise->set_exception( std::current_exception() );
        }
      } );
      return ret;
    }



    class DynTaskThreadPool::PoolWorker : boost::noncopyable
    {
    public:
      PoolWorker( DynTaskThreadPool* parent, const std::string& name );
      bool isbusy() const;
      void join();
      void run();
    private:
      std::string _name;
      bool _busy;
      std::thread _thread;
      DynTaskThreadPool* _parent;
      struct BusyGuard
      {
        bool *_busy;
        BusyGuard( bool* busy ) :_busy( busy ) { ( *_busy ) = true; }
        ~BusyGuard() { ( *_busy ) = false; }
      };
    };
    DynTaskThreadPool::PoolWorker::PoolWorker( DynTaskThreadPool* parent, const std::string& name ) :
      _name( name ), _busy( false ), _thread( ), _parent( parent )
    {
      run();
    }
    bool DynTaskThreadPool::PoolWorker::isbusy() const
    {
      return _busy;
    }

    void DynTaskThreadPool::PoolWorker::join()
    {
      _thread.join();
    }

    void DynTaskThreadPool::PoolWorker::run()
    {
      _thread = std::thread( [&]()
      {
        ThreadRegister register_thread( _name );
        auto f = msg();
        try
        {
          while ( !_parent->_done && !Clib::exit_signalled )
          {
            _parent->_msg_queue.pop_wait( &f );
            {
              BusyGuard busy( &_busy );
              f();
            }
          }
        }
        catch ( msg_queue::Canceled& )
        {
        }
        catch ( std::exception& ex )
        {
          ERROR_PRINT << "Thread exception: " << ex.what() << "\n";
          Clib::force_backtrace( true );
          return;
        }
      } );
    }

    /// Creates a dynamic threadpool of workers.
    /// if no idle worker is found creates a new worker thread
    /// blocks on deconstruction
    /// eg:
    /// DynTaskThreadPool workers;
    /// for (....)
    ///   workers.push([&](){dosomework();});
    DynTaskThreadPool::DynTaskThreadPool( const std::string& name )
      : _done( false ), _msg_queue( ), _pool_mutex( ), _name( "DynTaskPool"+name )
    {}

    size_t DynTaskThreadPool::threadpoolsize() const
    {
      std::lock_guard<std::mutex> guard( _pool_mutex );
      return _threads.size();
    }

    void DynTaskThreadPool::create_thread()
    {
      for ( const auto& worker : _threads )
      {
        if ( !worker->isbusy() ) // check for a idle instance
        {
          return;
        }
      }
      std::lock_guard<std::mutex> guard( _pool_mutex );
      size_t thread_num = _threads.size();
      _threads.emplace_back( new PoolWorker( this, _name + " " + fmt::FormatInt( thread_num ).str() ) );
      ERROR_PRINT << "create pool worker " << _name << " " << thread_num << "\n";
    }

    DynTaskThreadPool::~DynTaskThreadPool()
    {
      // send both done and cancel to wake up all workers
      _msg_queue.push( [&]()
      {
        _done = true;
        _msg_queue.cancel();
      } );
      for ( auto& thread : _threads )
        thread->join();
    }

    /// simply fire and forget only the deconstructor ensures the msg to be finished
    void DynTaskThreadPool::push( msg msg )
    {
      create_thread();
      _msg_queue.push( msg );
    }

    /// returns a future which will be set once the msg is processed
    std::future<bool> DynTaskThreadPool::checked_push( msg msg )
    {
      auto promise = std::make_shared<std::promise<bool>>();
      auto ret = promise->get_future();
      create_thread();
      _msg_queue.push( [=]()
      {
        try
        {
          msg();
          promise->set_value( true );
        }
        catch ( ... )
        {
          promise->set_exception( std::current_exception() );
        }
      } );
      return ret;
    }
  }
}