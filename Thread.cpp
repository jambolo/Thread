/** @file *//********************************************************************************************************

                                                      Thread.cpp

						                    Copyright 2003, John J. Bolton
	--------------------------------------------------------------------------------------------------------------

	$Header: //depot/Libraries/Thread/Thread.cpp#4 $

	$NoKeywords: $

 ********************************************************************************************************************/

#include "Thread.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <mmsystem.h>

#include "Misc/exceptions.h"
#include "Misc/max.h"
//#include "Misc/Trace.h"

#include <cassert>
#include <xutility>


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

//
/// @param	pContext	The parameter to Main().

Thread::Thread( void * pContext )
	: m_pContext( pContext )
{
	// Start the thread

	m_Handle = (HANDLE)_beginthreadex( NULL, 0, ThreadMain, this, 0, &m_ThreadIdentifier );
	if ( m_Handle == 0 )
	{
		throw ConstructorFailedException();
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// Derived classes may want to use this constructor in order to use a custom thread function. This constructor
/// does not start a thread.

Thread::Thread()
{
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @note	Classes derived from Thread must call End() before deleting any data that the thread might be
///			accessing.
/// @note	~Thread() waits for the thread to finish.

Thread::~Thread()
{
	// Wait for the thread to finish, then release the handle.
	End();
	CloseHandle( m_Handle );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

void Thread::Suspend()
{
	SuspendThread( m_Handle );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

void Thread::Resume()
{
	ResumeThread( m_Handle );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @param	wait	If @a wait is @c true, then End() will wait for the thread to exit.
///					If @a wait is @c false, then End() will forcibly terminate the thread and return immediately.
///					Note that a thread terminated in this manner does not release its resources. 

void Thread::End( bool wait/* = true */ )
{
	if ( wait )
	{
		// Wait for the thread to finish

		WaitForSingleObject( m_Handle, INFINITE );
	}
	else
	{
		// Terminate the thread immediately

		TerminateThread( m_Handle, 0 );
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

unsigned __stdcall Thread::ThreadMain( void * pArg )
{
	Thread * const	pThread	= reinterpret_cast< Thread * >( pArg );

	pThread->Main( pThread->m_pContext );

	return 0;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

ActivatedThread::ActivatedThread()
{
	// Initialize the critical section for passing commands

	InitializeCriticalSection( &m_ContextCriticalSection );

	// Create a "manual reset" command event to be used as a semaphore.

	m_ActivationEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( m_ActivationEvent == NULL )
	{
		DeleteCriticalSection( &m_ContextCriticalSection );
		throw ConstructorFailedException();
	}

	// Start the thread

	m_Handle = (HANDLE)_beginthreadex( NULL, 0, ThreadMain, this, 0, &m_ThreadIdentifier );
	if ( m_Handle == 0 )
	{
		CloseHandle( m_ActivationEvent );
		DeleteCriticalSection( &m_ContextCriticalSection );
		throw ConstructorFailedException();
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @note	Classes derived from ActivatedThread must call End() before deleting any data that the thread might be
///			accessing. ~ActivatedThread() automatically calls End(). Calling End() multiple times is ok.

ActivatedThread::~ActivatedThread()
{
	// Tell the thread to exit and wait for it.

	End();

	// Release the synchronization handles

	CloseHandle( m_ActivationEvent );
	DeleteCriticalSection( &m_ContextCriticalSection );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

///
/// @param	pContext	Data to send to the thread
///
/// @warning	Calls to Activate() are not queued. That is, if Activate() is called a second time before the thread
///				has responded to the first time, the first call will be lost. 


void ActivatedThread::Activate( void * pContext )
{
	EnterCriticalSection( &m_ContextCriticalSection );

	SetContext( pContext );
	SetEvent( m_ActivationEvent );

	LeaveCriticalSection( &m_ContextCriticalSection );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @param	wait	If @a wait is @c true, then End() will tell the thread to exit, and then wait for it to exit.
///					If @a wait is @c false, then End() will forcibly terminate the thread and return immediately.
///					Note that a thread terminated in this manner does not release its resources. 

void ActivatedThread::End( bool wait/* = true */ )
{
	if ( wait )
	{
		// Tell the thread to exit

		Activate( &m_ExitCommand );

		// Wait for it

		WaitForSingleObject( m_Handle, INFINITE );
	}
	else
	{
		TerminateThread( m_Handle, 0 );
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

unsigned __stdcall ActivatedThread::ThreadMain( void * pArg )
{
	ActivatedThread * const	pThread	= reinterpret_cast< ActivatedThread * >( pArg );

	void *	pContext;							// New context
	bool	done			= false;

	// Loop until an exit command is received, or an error occurs, or Main returns true

	while ( !done )
	{
		// Wait for the next activation

		DWORD const	signal	= WaitForSingleObject( pThread->m_ActivationEvent, INFINITE );
		
		switch ( signal )
		{
		case WAIT_ABANDONED:
			pContext = &m_ExitCommand;		// The main thread has terminated, so should we
			break;

		case WAIT_OBJECT_0:
			EnterCriticalSection( &pThread->m_ContextCriticalSection );

			pContext = pThread->GetContext();				// Get the context parameter
			pThread->SetContext( 0 );						// Reset it so we know that we have it
			ResetEvent( pThread->m_ActivationEvent );		// Reset the event so we can be activated again

			LeaveCriticalSection( &pThread->m_ContextCriticalSection );
			break;
			
		case WAIT_TIMEOUT:
//			trace( "ThreadMain - WaitForSingleObject() returned WAIT_TIMEOUT, but wasn't supposed to.\n" );
			pContext = &m_ExitCommand;
			break;
			
		case WAIT_FAILED:
//			trace( "ThreadMain - WaitForSingleObject() returned WAIT_FAILED, error = 0x%x\n", GetLastError() );
			pContext = &m_ExitCommand;
			break;
			
		default:
			assert( false );		// All cases should be accounted for
			pContext = &m_ExitCommand;
			break;
		}

		// Do main thread function unless the thread should exit

		if ( pContext != &m_ExitCommand )
		{
			done = pThread->Main( pContext );
		}
		else
		{
			done = true;
		}
	}

	return 0;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @param	period		Main() is executed every @a period ticks (if possible).
/// @param	pContext	The parameter to Main().

PeriodicThread::PeriodicThread( int period, void * pContext/* = 0 */ )
	: m_SyncPeriod( period )
{
	assert( period > 0 && period != INFINITE );
	SetContext( pContext );

	// Create an event used to signal the thread to end.

	m_ActivationEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( m_ActivationEvent == NULL )
	{
		throw ConstructorFailedException();
	}

	// Start the thread

	m_Handle = (HANDLE)_beginthreadex( NULL, 0, ThreadMain, this, 0, &m_ThreadIdentifier );
	if ( m_Handle == 0 )
	{
		CloseHandle( m_ActivationEvent );
		throw ConstructorFailedException();
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @note	Classes derived from PeriodicThread must call End() before deleting any data that the thread might be
///			accessing. ~PeriodicThread() automatically calls End(). Calling End() multiple times is ok.

PeriodicThread::~PeriodicThread()
{
	// Tell the thread to exit and wait for it.

	End();

	// Release the synchronization handle

	CloseHandle( m_ActivationEvent );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// @param	wait	If @a wait is @c true, then End() will tell the thread to exit, and then wait for it to exit.
///					If @a wait is @c false, then End() will forcibly terminate the thread and return immediately.
///					Note that a thread terminated in this manner does not release its resources. 

void PeriodicThread::End( bool wait/* = true */ )
{
	if ( wait )
	{
		// Tell the thread to exit

		SetEvent( m_ActivationEvent );

		// Wait for it

		WaitForSingleObject( m_Handle, INFINITE );
	}
	else
	{
		TerminateThread( m_Handle, 0 );
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

unsigned __stdcall PeriodicThread::ThreadMain( void * pArg )
{
	PeriodicThread * const	pThread	= reinterpret_cast< PeriodicThread * >( pArg );

	bool			done			= false;
	DWORD			nextUpdateTime	= timeGetTime();			// The time at which the Main should be called again
	void * const	pContext		= pThread->GetContext();	// We need our own copy to prevent the main thread
																// from changing it.

	// Loop until an exit command is received or an error occurs, or Main says to

	while ( !done )
	{
		// Figure out how long to wait.

		long	timeTillNextUpdate;

		if ( pThread->m_SyncPeriod == INFINITE )
		{
			timeTillNextUpdate = INFINITE;
		}
		else
		{
			DWORD const		currentTime		= timeGetTime();

			timeTillNextUpdate	= std::max( long( nextUpdateTime + pThread->m_SyncPeriod - currentTime ), 0L );
			nextUpdateTime		= currentTime + timeTillNextUpdate;
		}

		// Wait for the next update time, checking for commands from the main thread.

		DWORD const	signal	= WaitForSingleObject( pThread->m_ActivationEvent, timeTillNextUpdate );
		
		switch ( signal )
		{
		case WAIT_ABANDONED:
			done = true;		// The main thread has terminated, so should we
			break;

		case WAIT_OBJECT_0:
			// Any activation event means to terminate
			done = true;
			break;
			
		case WAIT_TIMEOUT:
			done = pThread->Main( pContext );
			break;
			
		case WAIT_FAILED:
//			trace( "ThreadMain - WaitForSingleObject() returned WAIT_FAILED, error = 0x%x\n", GetLastError() );
			done = true;
			break;
			
		default:
			assert( false );		// All cases should be accounted for
			done = true;
			break;
		}
	}

	return 0;
}



