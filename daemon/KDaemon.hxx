#pragma once
#define _WINSOCKAPI_
#include <string>

#include "..\shared\KWinsta.hxx"
#include "..\shared\KIni.hxx"
#include "..\shared\KTrace.hxx"
#include "..\shared\KSocketDup.hxx"
#include "..\shared\KSocket.hxx"
#include "..\shared\KSessionState.hxx"
#include "..\shared\KIPBan.hxx"
#include "..\shared\KDaemonParams.hxx"
#include ".\KTermSessions.hxx"


class KDaemon
{
private:
	// =============================================================================
	// vars
	// =============================================================================
	KTermSessions sessions;
	SERVICE_STATUS status;
	SERVICE_STATUS_HANDLE statusHandle;
	HANDLE worker;

	KDaemonParams params;
private:
	inline static std::string iniFileName;
	inline static bool isRunningWithoutService = false;
	inline static std::string winStaName = "";

	// =============================================================================
	// params
	// =============================================================================
/*
	struct Params
	{
		std::string service_name;
		std::string service_info;

		unsigned trace_level;
		std::string error_file;
		std::string trace_file;
		std::string log_file;

		std::string active_sessions_dir;

		int port;
		std::string ip;
		int max_sessions;
		bool debug_flag;
		bool use_ssh;

		Params( )
		{
			KIni ini;
			ini.File(KTS_INI_FILE);

			ini.GetKey( "KDaemon", "service_name", this->service_name );
			ini.GetKey( "KDaemon", "service_info", this->service_info );

			ini.GetKey( "KDaemon", "trace_level", this->trace_level );
			ini.GetKey( "KDaemon", "error_file", this->error_file );
			ini.GetKey( "KDaemon", "trace_file", this->trace_file );
			ini.GetKey( "KDaemon", "log_file", this->log_file );

			ini.GetKey( "KDaemon", "port", this->port );
			ini.GetKey( "KDaemon", "ip", this->ip );
			ini.GetKey( "KDaemon", "max_sessions", this->max_sessions );

			ini.GetKey( "KDaemon", "debug_flag", this->debug_flag );

			ini.GetKey( "KDaemon", "use_ssh", this->use_ssh );

			ini.GetKey( "KSession", "active_sessions_dir", this->active_sessions_dir );

			KWinsta::ExpandEnvironmentString( this->active_sessions_dir );
			KWinsta::ExpandEnvironmentString( this->log_file );
			KWinsta::ExpandEnvironmentString( this->trace_file );
			KWinsta::ExpandEnvironmentString( this->error_file );
		}
	} params;
*/
private:
	// =============================================================================
	// event handler so we don't die on console events
	// =============================================================================
	static BOOL WINAPI event_handler( DWORD event1 )
	{
		ktrace_in( );
		ktrace( "KDaemon::event_handler( " << event1 << " )" )
		return( true );
	}

private:
	// =============================================================================
	// constructor / private!
	// =============================================================================
	KDaemon( )
	{
		if(KDaemon::iniFileName.empty())
		{
			KDaemon::iniFileName = KTS_INI_FILE;
//			params = KDaemonParams(KTS_INI_FILE);
		}
//		else
//		{
			params = KDaemonParams(KDaemon::iniFileName);
//		}
		ktrace_master_level( this->params.trace_level );
		ktrace_error_file( this->params.error_file );
		ktrace_trace_file( this->params.trace_file );
		ktrace_log_file( this->params.log_file );

		ktrace_in( );
		ktrace_level( 10 );

		ktrace( "KDaemon::KDaemon( )" );

		klog( "daemon.exe started with parameters from " << KDaemon::iniFileName );

		SetConsoleCtrlHandler( KDaemon::event_handler, true );
	}

private:
	// =============================================================================
	// instance
	// =============================================================================
	static KDaemon * instance( )
	{
		static KDaemon * _instance = 0;

		if( _instance ) return( _instance );

		_instance = new( KDaemon );

		return( _instance );
	}

private:
	// =============================================================================
	// service control handler
	// =============================================================================
	static void _stdcall ServiceCtrlHandler( DWORD opcode )
	{
		ktrace_in( );
		ktrace( "KDaemon::ServiceCtrlHandler( " << opcode << " )" );

		switch( opcode )
		{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			klog( "stopping service" );

			// kill the main thread
			if( KDaemon::instance( )->worker == 0 )
			{
				kerror( "no handle to worker thread/disregard if running on NT4/: err" );
				ExitProcess(0);
			}
			else
			{
				if( !TerminateThread( KDaemon::instance( )->worker, 0 ) )
				{
					kerror( "can't terminate worker thread[" << KDaemon::instance( )->worker << "]: err" );
				}
			}

			KDaemon::instance( )->status.dwWin32ExitCode = 0;
			KDaemon::instance( )->status.dwCurrentState = SERVICE_STOPPED;
			KDaemon::instance( )->status.dwCheckPoint = 0;
			KDaemon::instance( )->status.dwWaitHint = 0;

			if( !SetServiceStatus( KDaemon::instance( )->statusHandle, &KDaemon::instance( )->status ) )
			{
				ktrace( "SetServiceStatus( ):err" );
			}
			return;

		case SERVICE_CONTROL_INTERROGATE: 
			// Fall through to send current status. 
			break;
		default: 
			ktrace( "unsupported " << opcode );
			break;
		}

		if( !SetServiceStatus( KDaemon::instance( )->statusHandle, &KDaemon::instance( )->status ) )
		{
			ktrace( "SetServiceStatus( ):err" );
		}
		return; 
	}

private:
	// =============================================================================
	// init service
	// =============================================================================
	bool InitService( )
	{
		ktrace_in( );
		ktrace( "KDaemon::InitService( )" );
		
		KSessionState ss;
		ss.DeleteAllSessions( this->params.active_sessions_dir );

		if( this->params.debug_flag ) return( true );
		if(KDaemon::isRunningWithoutService) return(true);

		this->worker = KWinsta::OpenThread( THREAD_ALL_ACCESS, false, GetCurrentThreadId( ) );
		this->status.dwServiceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
		this->status.dwCurrentState = SERVICE_START_PENDING;
		this->status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		this->status.dwWin32ExitCode = 0;
		this->status.dwServiceSpecificExitCode = 0;
		this->status.dwCheckPoint = 0;
		this->status.dwWaitHint = 0;

		this->statusHandle = RegisterServiceCtrlHandler( this->params.service_name.c_str( ), KDaemon::ServiceCtrlHandler );
		
		if( !this->statusHandle )
		{
			ktrace( "RegisterServiceCtrlHandler( ):err" );
			return( false );
		}

		// set running status
		this->status.dwCurrentState = SERVICE_RUNNING;
		this->status.dwCheckPoint = 0;
		this->status.dwWaitHint = 0;

		if( !SetServiceStatus( this->statusHandle, &this->status ) )
		{
			ktrace( "SetServiceStatus( ):err" );
			return( false );
		}
		return( true );
	}

private:
	// =============================================================================
	// service main
	// =============================================================================
	static void _stdcall ServiceMain( unsigned long argc, char** argv )
	{

		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KDaemon::ServiceMain( )" );

		klog( "starting service with parameters from " << KDaemon::iniFileName);

//		KDaemon::iniFileName = KTS_INI_FILE;

		if (argc > 1)
		{
			if ((strcmp("-config", argv[1]) == 0) && (argc >= 3))
			{
				KDaemon::iniFileName = argv[2];
				klog("ini file set to " << KDaemon::iniFileName);
			}
		}

		if( !KDaemon::instance( )->InitService( ) )
		{
			kerror( "KDaemon::instance( )->InitService( ):err" );
			return;
		}

		if( KSocket::WSAStartup( ) != 0 )
		{
			kerror( "KSocket::WSAStartup( ):err" );
			return;
		}

		// create the kts desktop
		KDaemon::winStaName = KWinsta::CreateWinstaAndDesktop( );
		if(KDaemon::winStaName.empty())
		{
			kerror( "KWinsta::CreateWinstaAndDesktop( ):err" );

			if( !KDaemon::instance( )->params.debug_flag ) return;
		}

		KDaemon::instance( )->AcceptConnections( );

		kend_try_block;
	}

private:
	// =============================================================================
	// accept connections
	// =============================================================================
	void AcceptConnections( )
	{
		ktrace_in( );
		ktrace( "KDaemon::AcceptConnections( )" );

		KSocket sock;
		if( !sock.Listen( KDaemon::instance( )->params.port, KDaemon::instance( )->params.ip ) )
		{
			kerror( "sock.Listen( " << KDaemon::instance( )->params.port << " ):err" );
			return;
		}

		KIPBan ip_ban(KDaemon::iniFileName);

		while( true )
		{
			KSocket tmp = sock.Accept( );

			if( tmp.GetSocket( ) == INVALID_SOCKET )
			{
				kerror( "sock.Accept( ):err" );
				return;
			}

			klog( "KTS connected to " << tmp.GetConnectionIP( ) << ":" << tmp.GetConnectionPort( ) );

			if( ip_ban.IsBanned( tmp.GetConnectionIP( ) ) )
			{
				klog( "ip is banned" );
				tmp.Close( );
				continue;
			}

			if( this->sessions.Count( ) >= this->params.max_sessions )
			{
				klog( "max_sessions reached" );
				tmp.Close( );
				continue;
			}

			std::string curDir = KWinsta::GetCurrentDirectory();
			klog("working in directory: " << curDir);

			STARTUPINFO si = {0};
			PROCESS_INFORMATION pi = {0};

			std::stringstream s;
			s << curDir
			    << "\\shlex.exe \""
			    << curDir << "\\session.exe\" \""
				<< " -ppid:" << GetCurrentProcessId()
				<< " -ip:" << tmp.GetConnectionIP()
				<< " -port:" << tmp.GetConnectionPort();

			if( this->params.debug_flag ) s << " -debug:1";
			if( this->params.use_ssh ) s << " -ssh:1";
			else s << " -ssh:0";
			s << " -config:" << KDaemon::iniFileName;
			s << "\"";

			si.cb = sizeof(si);

			ktrace( "CreateProcess( " << s.str( ) << " )" );
			if( !CreateProcess( NULL, ( char * )s.str( ).c_str( ), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) )
			{
				kerror( "CreateProcess( ):err" );
				return;
			}

			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);


			//transfer the socket to spawned process
			KSocketDup ksd;

			ksd.SendSocket( tmp.GetSocket( ), "kts" + tmp.GetConnectionIP( ) + "_" + tmp.GetConnectionPort( ) );

			this->sessions.Add( ksd.pid );

			tmp.Close( false );
		}
	}

public:
	// =============================================================================
	// run service
	// =============================================================================
	static void RunService( std::string inifile )
	{
		ktrace_in( );
		ktrace( "KDaemon::RunService( inifile )" );
		KDaemon::isRunningWithoutService = false;
		KDaemon::iniFileName = inifile;

		SERVICE_TABLE_ENTRY service[] =
		{
			{ ( char * )KDaemon::instance( )->params.service_name.c_str( ), KDaemon::ServiceMain },
			{ 0, 0 }
		};
		if( !StartServiceCtrlDispatcher( service ) )
		{
			kerror( "StartServiceCtrlDispatcher( ):err" );
		}
	}

public:
	// =============================================================================
	// debug
	// =============================================================================
	static void Debug()
	{
		KDaemon::Debug(KTS_INI_FILE);
	}
	
	static void Debug(char* inifile)
	{
		ktrace_in( );
		ktrace( "KDaemon::Debug(inifile)" );
		KDaemon::iniFileName = inifile;
		KDaemon::isRunningWithoutService = true;
		KDaemon::instance( )->params.debug_flag = true;
		int _argc = 3;
		char* _argv[3];
		_argv[0] = EXE_DAEMON;
		_argv[1] = "-config";
		_argv[2] = inifile;
		KDaemon::ServiceMain(_argc, _argv);
	}

	static void Run()
	{
		KDaemon::Run(KTS_INI_FILE);
	}

	static void Run(char* inifile)
	{
		ktrace_in();
		ktrace("KDaemon::Run(inifile)");
		KDaemon::iniFileName = inifile;
		KDaemon::isRunningWithoutService = true;
//		KDaemon::instance()->params.debug_flag = true;

		int _argc = 3;
		char* _argv[3];
		_argv[0] = EXE_DAEMON;
		_argv[1] = "-config";
		_argv[2] = inifile;
		KDaemon::ServiceMain(_argc, _argv);
	}
};