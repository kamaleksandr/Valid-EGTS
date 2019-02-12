/* 
 * File:   common.h
 * Author: kamyshev.a
 *
 * Created on August 29, 2018, 11:23 am
 */

#ifndef EGTS_COMMON_H
#define EGTS_COMMON_H

#if _WIN32
#include <windows.h>
#define MUTEX_TYPE HANDLE
#define MUTEX_SETUP(x) (x) = CreateMutex(NULL, FALSE, NULL)
#define MUTEX_CLEANUP(x) CloseHandle(x)
#define MUTEX_LOCK(x) WaitForSingleObject((x), INFINITE)
#define MUTEX_UNLOCK(x) ReleaseMutex(x)
#define THREAD_ID GetCurrentThreadId()
#else
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()
#endif

#define EGTS_SBR_HDR_SIZE sizeof( subrec_header_t )

#include <stdint.h>
#include <string.h>
#include <memory>
#include <deque>
#include "egts_defines.h"

namespace EGTS
{

struct error_t
{
  uint32_t code;
  std::string sender;
  std::string message;
};

typedef void (*error_callback_t)(error_t*) ;

struct error_handler_t
{
  error_callback_t OnError;
  EGTS::error_t error;
#ifdef THREADSAFE_MODE
  error_handler_t( )
  {
    MUTEX_SETUP( mutex );
    OnError = 0;
  }
  ~error_handler_t( ){ MUTEX_CLEANUP( mutex ); }
  MUTEX_TYPE mutex;
#else
  error_handler_t( ){ OnError = 0; }
#endif
};

void SetErrorCallback( error_callback_t );
void HandleAnError( error_t &error );

#pragma pack( push, 1 )
struct subrec_header_t
{
  uint8_t type;
  uint16_t len;
};
#pragma pack( pop )

struct data_size_t
{
  data_size_t( ){ size = 0; }
  std::unique_ptr<char> data;
  uint16_t size;
};

template<class T>
class object_list_t
{
private:
  std::deque< T* > list;
  typename std::deque< T* >::iterator it;
public:
  object_list_t( ){ it = list.end( ); }
  ~object_list_t( ){ Clear( ); }
  uint32_t Count( ){ return list.size( ); }
  void Clear( )
  {
    for ( First( ); it != list.end( ); Next( ) ) delete *it;
    list.clear( );
    it = list.end( );
  }
  T* New( )
  {
    try { list.push_back( new T ); }
    catch ( std::exception &e )
    {
      EGTS::error_t error = {0, "object_list_t", e.what( )};
      HandleAnError( error );
      return 0;
    }
    return list.back( );
  }
  T* Add( T *t )
  {
    list.push_back( t );
    return list.back( );
  }
  T* First( )
  {
    it = list.begin( );
    if ( it == list.end( ) ) return 0;
    return *it;
  }
  T* Next( )
  {
    if ( it == list.end( ) ) return 0;
    if ( ++it == list.end( ) ) return 0;
    return *it;
  }
  T* Erase( ) // deletes current, returns next or 0
  {
    if ( it == list.end( ) ) return 0;
    delete *it;
    it = list.erase( it );
    if ( it == list.end( ) ) return 0;
    return *it;
  }
};
}
#endif /* COMMON_H */



