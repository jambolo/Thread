#if !defined( THREAD_H_INCLUDED )
#define THREAD_H_INCLUDED

#pragma once

/** @file *//********************************************************************************************************

                                                       Thread.h

						                    Copyright 2003, John J. Bolton
	--------------------------------------------------------------------------------------------------------------

	$Header: //depot/Libraries/Thread/Thread.h#4 $

	$NoKeywords: $

 ********************************************************************************************************************/


#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// A basic thread.

///
/// This thread executes immediately and only once.

class Thread
{
public:

	/// Constructor
	Thread( void * pContext );

	/// Destructor
	virtual ~Thread();

	/// Supends the thread.
	void Suspend();

	/// Resumes the suspended thread.
	void Resume();

	/// Terminates the thread or waits for it to finish.
	void End( bool wait = true );

	HANDLE		m_Handle;				///< Thread handle
	unsigned	m_ThreadIdentifier;		///< Thread identifier

protected:

	/// Constructor
	Thread();

	/// Thread body. The return value is ignored.

	/// This is the function that actually does the work and you must overload it. Main() is called indirectly by
	/// the constructor.
	///
	/// @param	pContext		Pointer to context specified by Activate(), or 0.
	///
	virtual bool Main( void * pContext ) = 0;

	/// Sets the context used by the thread
	void SetContext( void * pContext )		{ m_pContext = pContext; }

	/// Returns the context used by the thread
	void * GetContext() const				{ return m_pContext; }

private:

	/// The thread function.
	static unsigned __stdcall ThreadMain( void * pArg );

	void * volatile 	m_pContext;					///< Thread context parameter
};


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// A thread that executes a function whenever told to (activated).

class ActivatedThread : public Thread
{
public:

	/// Constructor
	ActivatedThread();

	/// Destructor
	virtual ~ActivatedThread();

	/// @name	Overrides Thread
	//@{
	//	void Suspend();
	//	void Resume();
	//	virtual bool Main( void * pContext ) = 0;
	//	void SetContext( void * pContext );
	//	void * GetContext() const;
	void End( bool wait = true );
	//@}

	/// Activates the thread.
	void Activate( void * pContext );

private:

	/// The thread function.
	static unsigned __stdcall ThreadMain( void * pArg );

	HANDLE				m_ActivationEvent;			///< Thread activation event handle
	CRITICAL_SECTION	m_ContextCriticalSection;	///< Critical section guarding context parameter passing
	static int			m_ExitCommand;				///< When address of this is sent as the context parameter, the
													///< thread will exit. It's value is never used.
};


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

/// A thread that executes a function multiple times at a specified rate.

class PeriodicThread : public Thread
{
public:

	/// Constructor
	explicit PeriodicThread( int period, void * pContext = 0 );

	/// Destructor
	virtual ~PeriodicThread();

	/// @name	Overrides Thread
	//@{
	//	void Suspend();
	//	void Resume();
	//	virtual bool Main( void * pContext ) = 0;
	//	void SetContext( void * pContext );
	//	void * GetContext() const;
	void End( bool wait = true );
	//@}

private:

	/// The thread function.
	static unsigned __stdcall ThreadMain( void * pArg );

	HANDLE	m_ActivationEvent;			///< Thread activation event handle
	int		m_SyncPeriod;				///< Maximum time between calls to Main()
};


#endif // !defined( THREAD_H_INCLUDED )
