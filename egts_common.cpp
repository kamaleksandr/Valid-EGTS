/* 
 * File:   common.cpp
 * Author: kamyshev.a
 * 
 * Created on August 29, 2018, 11:23 am
 */

#include "egts_common.h"

static egts_error_handler_t error_handler;

void EGTSSetErrorCallback( egts_error_callback_t cb )
{
  error_handler.OnError = cb;
}

void EGTSHandleAnError( egts_error_t &error )
{
#ifdef THREADSAFE_MODE
  MUTEX_LOCK( error_handler.mutex );
#endif
  error_handler.error.code = error.code;
  error_handler.error.sender = error.sender;
  error_handler.error.message = error.message;
  if ( error_handler.OnError ) error_handler.OnError( &error_handler.error );
#ifdef THREADSAFE_MODE
  MUTEX_UNLOCK( error_handler.mutex );
#endif
}


