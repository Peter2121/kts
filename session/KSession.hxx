#pragma once

#define NOCRYPT        /* cryptlib nedds this to work with windows headers */
#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */

#include "..\shared\kts.h"
#include "..\shared\KWinsta.hxx"
#include "..\shared\KTrace.hxx"
#include "..\shared\KSocket.hxx"
#include "..\shared\KSocketDup.hxx"
#include "..\shared\KConsole.hxx"
#include "..\shared\KKey.hxx"
#include "..\shared\KScreenExport.hxx"
#include "..\shared\KPrcsGroup.hxx"
#include "..\shared\KFlags.hxx"
#include "..\shared\KPipe.hxx"
#include "..\shared\KSsh.hxx"
#include "..\shared\KSessionState.hxx"
#include "..\shared\KIPBan.hxx"
#include "..\shared\KSessionParams.hxx"
#include ".\KPortForward.hxx"
#include ".\KSftp.hxx"
#include ".\KCommUtils.hxx"
#include ".\KAutoLogon.hxx"
#include ".\KPublickeyLogon.hxx"
#include ".\KSubSystems.hxx"
class KSession
{
#define IDLE_TIMEOUT "idle_timeout"
#define PASS_TIMEOUT "pass_timeout"
	const std::string STATE_CLOSED = "closed";
	/*==============================================================================
	 * var
	 *=============================================================================*/
private:
	enum Status
	{
		  LoginPhase = 0
		, Active
		, Terminate
		, Banner
	} status;

	PROCESS_INFORMATION shell;

	// object pointers
	KConsole * console;
	KKey * key;
	KTelnet * telnet;
	KScreenExport * export1;
	KPrcsGroup * shell_group;
	KSsh * ssh;
	KSocket * sock;
	KFlags * flags;
	KSessionState * session_state;
	KIPBan * ip_ban;
	KPipe * pipe;
	KAutoLogon * auto_logon;
	KPublickeyLogon * publickey_logon;
	KSubSystems * subsystems;


	bool password_must_change;
	bool is_ssh;
	std::string pipe_name;
	std::string ssh_first_read;
	std::string iniFileName;
	const std::string DESKTOP_NAME = "ktsDESK";
	std::string winStaName;

	// user params
	std::string user;
	HANDLE token;

	// service process handle
	HANDLE service;

	HANDLE shell_out;

	CRITICAL_SECTION cs;

private:
	KSessionParams params;

private:
	// =============================================================================
	// event handler so we don't die on console events
	// =============================================================================
	static BOOL WINAPI event_handler( DWORD event1 )
	{
		USE( event1 );
// memory leak ? wtf ?
//		ktrace_in( );
//		ktrace( "KSession::event_handler( " << event1 << " )" )
		return( true );
	}

public:
	/*==============================================================================
	 * constructors
	 *=============================================================================*/
//	KSession( bool is_ssh ) : KSession( is_ssh, KTS_INI_FILE ) {}
	KSession( bool is_ssh, std::string inifile ) : params(inifile)
	{
		this->iniFileName = inifile;
		this->winStaName = "";
		ktrace_master_level( this->params.trace_level );
		ktrace_error_file( this->params.error_file );
		ktrace_trace_file( this->params.trace_file );
		ktrace_log_file( this->params.log_file );


		ktrace_in( );
		ktrace_level( 10 );
		ktrace( "KSession::KSession( )" );

		klog( "session.exe started with config file " << this->iniFileName );

		ZeroMemory( &this->shell, sizeof( this->shell ) );
		this->is_ssh = is_ssh;

		this->password_must_change = false;

		// create objects
		this->CreateObjects( );

		InitializeCriticalSection( &this->cs );

		// set ctrl handler
		SetConsoleCtrlHandler( KSession::event_handler, true );

		KSocket::WSAStartup( );

		//debug flag
		this->params.debug_flag = false;
		std::stringstream( KWinsta::GetCmdLineParam( "-debug:" ) ) >> this->params.debug_flag;

		// create flags
		this->flags->Create( "idle_timeout", this->params.idle_timeout );
		this->flags->Create( "pass_timeout", this->params.pass_timeout );
		this->flags->Create( "net_check_delay", this->params.net_check_delay );
		this->flags->Create( "sock_io_timeout", this->params.io_timeout );
		this->flags->Create( "pipe_io_timeout", this->params.io_timeout );
		this->flags->Create( "proxy" );
		this->flags->Create( "shell" );

		// get parent(service) pid
		DWORD pid;
		std::stringstream( KWinsta::GetCmdLineParam( "-ppid:" ) ) >> pid;
		this->service = KWinsta::PidToHandle( pid );
		if( this->service == 0 )
		{
			kerror("KWinsta::PidToHandle:err " << GetLastError( ));
		}
	}

private:
	// =============================================================================
	// create objects (the stack is too narrow for all of us )
	// =============================================================================
	void CreateObjects( )
	{
		ktrace_in( );
		ktrace( "KSession::CreateObjects( )" );

		if( this->is_ssh ) 
		{
			this->ssh = new KSsh( true );
			this->ssh->SetAlgoLists(
				    this->params.kex_algo_list
				  , this->params.encr_algo_list
				  , this->params.mac_algo_list);
		}
		this->telnet = new KTelnet( this->is_ssh );
		this->export1 = new KScreenExport( *telnet );
		this->console = new KConsole( );
		this->key = new KKey(this->iniFileName);
		this->shell_group = new KPrcsGroup( );
//		this->sock = new KSocket( );
		this->flags = new KFlags( );
		this->session_state = new KSessionState( );
		this->ip_ban = new KIPBan( this->iniFileName );
		this->pipe = new KPipe( );
		this->auto_logon = new KAutoLogon( );
		this->publickey_logon = new KPublickeyLogon( );
		this->subsystems = new KSubSystems( );
	}

private:
	// =============================================================================
	// init the session
	// =============================================================================
	void Init()
	{
		ktrace_in( );
		ktrace( "KSession::Init( )" );

		// create timeout monitor threads
		CreateThread( NULL, 0, KSession::HealthMonitorThread, ( LPVOID )this, 0, NULL );

		// get the socket
		KSocketDup dup;
		this->pipe_name = "kts" + this->params.client_ip + "_" + this->params.client_port;
		this->flags->Enable( "pipe_io_timeout" );
		this->sock = new KSocket( dup.GetSocket( this->pipe_name ) );	// frozen here
		this->flags->Disable( "pipe_io_timeout" );
		if( this->sock->GetSocket( ) == INVALID_SOCKET )
		{
			kerror( "sock:err" );
			klog( "session.exe terminated" );
			ExitProcess(0);
		}

		klog( "connected to " << this->sock->GetConnectionIP( ) << ":" << this->sock->GetConnectionPort( ) );

		// add this connection to ip ban module
		this->ip_ban->AddIPBanConnection( this->sock->GetConnectionIP( ) );

		this->session_state->SetSessionDirectory( this->params.active_sessions_dir );
		this->session_state->SetStateStarted( this->sock->GetConnectionIP( ), this->sock->GetConnectionPort( ) );

		this->subsystems->SetSubsystemDirectory( this->params.subsystems_dir );

		if( this->is_ssh )
		{
			if( !this->ssh->Init( this->sock->GetSocket( ), this->params.rsakey_file, "<default>", this->params.auto_create_rsa_key) )
			{
				kerror( "this->ssh.Init( ):err" );
			}
			klog( "ssh initialized" );
		}

		COORD screen = { this->params.screen_width, this->params.screen_height };
		this->console->SetScreenSize( screen );
		this->telnet->screen = screen;
		screen.Y = this->params.buff_height;
		this->console->SetBuffSize( screen );


		this->status = Banner;

		if( this->console->GetWindowHandle( ) )
		{
			this->key->SetWindow( this->console->GetWindowHandle( ) );
		}
		else
		{
			kerror( "this->console->GetWindowHandle( ):err" );
		}

		ktrace_level( 0 );
		this->key->Load( this->params.key_init );
		this->telnet->Load( this->params.telnet_init );
		this->auto_logon->Load( this->params.auto_logon_init );
		this->publickey_logon->Load( this->params.publickey_logon_init );
		ktrace_level( 10 );

		this->export1->dumb_client = this->params.dumb_client;
	}

private:
	// =============================================================================
	// create pipe or die miserably
	// =============================================================================
	void CreatePipe( )
	{
		ktrace_in( );
		ktrace( "KSession::CreatePipe( )" );

		if( !this->pipe->Create( this->pipe_name ) )
		{
			ktrace_level( 10 );
			kerror( "can't create pipe " << this->pipe_name );

			this->TerminateSession( );
		}
	}

private:
	// =============================================================================
	// accept pipe or die miserably
	// =============================================================================
	void AcceptPipe( )
	{
		ktrace_in( );
		ktrace( "KSession::AcceptPipe( )" );

		this->pipe->Accept();
	}

private:
	// =============================================================================
	// close pipe
	// =============================================================================
	void ClosePipe( )
	{
		ktrace_in( );
		ktrace( "KSession::ClosePipe( )" );
		this->flags->Enable( "pipe_io_timeout" );

		this->pipe->Close( );

		this->flags->Disable( "pipe_io_timeout" );
	}

private:
	// =============================================================================
	// write pipe if fail accept pipe
	// =============================================================================
	void WritePipe( const std::string & buff )
	{
		ktrace_in( );
		ktrace( "KSession::WritePipe( " << (unsigned int)buff.length() << " )" );

		this->flags->Enable( "pipe_io_timeout" );

		if( !this->pipe->Write( buff ) )
		{
			this->flags->Disable( "pipe_io_timeout" );

			this->session_state->SetStateDisconnected( );

			ktrace( "disconnect: can't write pipe " << this->pipe_name );
			
			this->flags->Raise( "disconnect" );

			this->ClosePipe( );
			this->CreatePipe( );
			this->AcceptPipe( );

			this->session_state->SetStatePipe( );
		}

		this->flags->Disable( "pipe_io_timeout" );
	}

private:
	// =============================================================================
	// read pipe if fail accept pipe
	// =============================================================================
	void ReadPipe( std::string & buff )
	{
		ktrace_in( );
		ktrace( "KSession::ReadPipe( )" );

		buff = "";

		this->flags->Enable( "pipe_io_timeout" );

		if( !this->pipe->ReadA( buff ) )
		{
			this->flags->Disable( "pipe_io_timeout" );

			this->session_state->SetStateDisconnected( );

			ktrace( "disconnect: can't read pipe " << this->pipe_name );

			this->flags->Raise( "disconnect" );

			this->ClosePipe( );
			this->CreatePipe( );
			this->AcceptPipe( );

			this->session_state->SetStatePipe( );
		}
		this->flags->Disable( "pipe_io_timeout" );
	}

private:
	// =============================================================================
	// fix console screen size
	// =============================================================================
	void FixConsoleScreenSize( bool _is_ssh )
	{
		ktrace_in( );
		ktrace( "KSession::FixConsoleScreenSize( )" );

		if( _is_ssh )
		{
			this->ssh->ScreenSize();
			this->telnet->screen.X = this->ssh->screenWidth;
			this->telnet->screen.Y = this->ssh->screenHeight;
		}

		// fix console screen
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		this->console->GetConsoleInfo( csbi );
		if( this->console->HasScroll( csbi ) )
		{
			this->telnet->screen.X = min( this->telnet->screen.X, 150 );

			this->console->SetScreenSize( this->telnet->screen );

			if( csbi.srWindow.Right != this->telnet->screen.X - 1 )
			{
				COORD buff;
				buff.X = this->telnet->screen.X;
				buff.Y = 300;

				this->console->SetBuffSize( buff );
			}
		}
	}

public:
	/*==============================================================================
	 * consume input
	 *=============================================================================*/
	void Consume( std::string & input, std::string & output )
	{
		ktrace_in( );
		ktrace( "KSession::Consume( " << input << " )" );

		output.clear();

		std::string buff;
		buff.reserve( STRING_INIT_SIZE );

		while( input != "" )
		{
			buff.clear();

			if( this->telnet->Consume( input, buff ) )
			{
				output += buff;
				continue;
			}

			if( this->key->Consume( input ) ) continue;

			input = input.substr( 1 );
		}

		if( output != "" )
		{
			ktrace_level( 10 );
			ktrace( "consume : " + output );
		}
	}
private:
	/*==============================================================================
	 * export1 screen (pipe mode)
	 *=============================================================================*/
	std::string ExportPipe( )
	{
		ktrace_in( );
		ktrace( "KSession::ExportPipe( )" );

		DWORD bytes = 0;
		DWORD read = 0;
		if( !PeekNamedPipe( this->shell_out, NULL, 0, NULL, &bytes, NULL) )
		{
			ktrace( "PeekNamedPipe( ) : err " << GetLastError( ) );
			return( "" );
		}
		if( bytes > 0 )
		{
			std::string buff;
			buff.resize( bytes, '\x00' );

			if( !ReadFile( this->shell_out, ( void * )buff.c_str( ), bytes, &read, NULL ) )
			{
				ktrace( "ReadFile( ) : err " << GetLastError( ) );
				return( "" );
			}

			std::string s;
			s.assign( buff.c_str( ), read );

			return s;
		}

		return "";
	}

public:
	/*==============================================================================
	 * export1 screen
	 *=============================================================================*/
	std::string Export( )
	{
		ktrace_in( );
		ktrace( "KSession::Export( )" );

		if( this->flags->IsRaised("shell") )
		{
			if( this->params.pipe_mode ) return this->ExportPipe( );
			else return this->export1->Export( );
		}
		else 
		{
			return this->export1->Export( );
		}
	}

private:
	// =============================================================================
	// change the user password
	// =============================================================================
	void ChangeUserPassword( const std::string & user1, const std::string & pass )
	{
		ktrace_in( );
		ktrace( "KSession::ChangeUserPassword( " << user1 << ", " << "********" << " )" );

		std::string domain = "";
		std::string new_pass = "";
		std::string new_pass2 = "";

		this->console->SetConsoleMode( ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT );

		this->console->Write( "\r\n" );
		this->console->Write( "------------------------------------------------------" );
		this->console->Write( "\r\n" );
		this->console->Write( "The user's password must be changed before logging on." );
		this->console->Write( "\r\n" );
		this->console->Write( "------------------------------------------------------" );
		this->console->Write( "\r\n" );
		this->console->Write( "\r\n" );

		while( true )
		{
			this->console->SetConsoleMode( ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT );

			this->console->Write( "user: " + user1);
			this->console->Write( "\r\n" );

			this->console->Write( "domain server: " );
			this->console->Read( domain );
			this->console->Write( "\r\n" );

			this->console->SetConsoleMode( ENABLE_LINE_INPUT );
	
			this->console->Write( "new password: " );
			this->console->Read( new_pass );
			this->console->Write( "\r\n" );

			this->console->Write( "confirm new password: " );
			this->console->Read( new_pass2 );
			this->console->Write( "\r\n" );

			if(new_pass != new_pass2)
			{
				this->console->Write( "\r\n" );
				this->console->Write( "The passwords you typed do not match.");
				this->console->Write( "\r\n" );
				this->console->Write( "\r\n" );
				this->console->Write( "\r\n" );
			}
			else
			{
				NET_API_STATUS _status = KWinsta::ChangePassword(domain, user, pass, new_pass );

				this->console->Write( "\r\n" );
				this->console->Write( KWinsta::GetErrorMessage(_status) );
				this->console->Write( "\r\n" );
				this->console->Write( "\r\n" );

				if( _status == NERR_Success ) 
				{
					klog( "password changed" );

					int _key = 0;
					this->console->ReadKey( _key );
					this->console->ReadKey( _key );

					return;
				}
			}
		}
	}

private:
	// =============================================================================
	// logon user and start the shell
	// =============================================================================
	// TODO: block login of different user in application mode *********************
	DWORD LogonUser1( const std::string & user1, const std::string & pass )
	{
		DWORD error;
		ktrace_in( );
		ktrace( "KSession::LogonUser1( " << user1 << ", " << "********" << " )" );

		this->params.user = user1;
		std::string _user = user1;

		// make user lowercase, thanks to Net147 for finding this bug
		KWinsta::ToLower( _user );

//		// check allowed users
//		KWinsta::ToLower( this->params.allowed_login_list );
//		if( this->params.allowed_login_list != "" && this->params.allowed_login_list.find( ":" + user + ":" ) == std::string::npos )
//		{
//			klog( "user not in allowed list" );
//			user = ":" + user;
//		}

//		// check forbiden users
//		KWinsta::ToLower( this->params.fordbiden_login_list );
//		if( this->params.fordbiden_login_list != "" && this->params.fordbiden_login_list.find( ":" + user + ":" ) != std::string::npos )
//		{
//			klog( "user in forbidden list" );
//			user = ":" + user;
//		}

		if( this->params.default_domain != "" )
		{
			_user = _user + "@" + this->params.default_domain;
		}

		SetEnvironmentVariable( "KTS_USER", this->params.user.c_str( ) );

		this->winStaName = KWinsta::SetWinstaAndDesktop( );
		if (this->winStaName.empty())
		{
			kerror("KWinsta::SetWinstaAndDesktop( kts ):err");
			this->winStaName = KWinsta::CreateWinstaAndDesktop();		
			if (this->winStaName.empty())
			{
				kerror("KWinsta::CreateWinstaAndDesktop( kts ):err");
				return (-1);
			}
		}

		if( ::LogonUser( _user.c_str( ), NULL, pass.c_str( ), LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &this->token ) )
		{
			this->session_state->SetStateLogged( _user );

			this->console->Write( this->params.login_successfull_message );

			klog( "login accepted: [ " << _user << " ]" );

			// release ip ban
			this->ip_ban->ResetIPBanConnection( this->sock->GetConnectionIP( ) );

   			return( ERROR_SUCCESS );
		}

		error = GetLastError( );
		if( error == ERROR_PASSWORD_MUST_CHANGE ) 
		{
			klog( "password must change: [ " << _user << " ]" );

			// release ip ban
			this->ip_ban->ResetIPBanConnection( this->sock->GetConnectionIP( ) );

			this->password_must_change = true;

			return( ERROR_SUCCESS );
		}

		if( this->params.debug_flag )
		{
				this->session_state->SetStateLogged( _user );
				return( ERROR_SUCCESS );
		}
		else
		{
				return( error );
		}
	}

	//
	//	Logon user, authenticated by public key, without password
	//
	DWORD LogonUserWithoutPassword(const std::string & user1)
	{
		ktrace_in();
		ktrace("KSession::LogonUserWithoutPassword( " << user1 << " )");

		char buf[UNLEN + 1];
		DWORD buflen = UNLEN + 1;
		std::string curr_user = "";

		if (KWinsta::IsLocalSystem())
		{
			// Not yet implemented
			kerror("Not yet implemented");
			return (-1);
		}
		else
		{
			this->params.user = user1;
			SetEnvironmentVariable("KTS_USER", this->params.user.c_str());
			if (!GetUserName(buf, &buflen))
			{
				kerror("cannot get current user name: " << GetLastError());
				return -1;
			}
			curr_user = buf;
			KWinsta::ToLower(curr_user);
			if(user1 != curr_user)
			{
				kerror("user name does not match: " << user1 << " : " << curr_user);
				return -1;
			}

			this->winStaName = KWinsta::SetWinstaAndDesktop();
			if (this->winStaName.empty())
			{
				kerror("KWinsta::SetWinstaAndDesktop( kts ):err");
				this->winStaName = KWinsta::CreateWinstaAndDesktop();
				if (this->winStaName.empty())
				{
					kerror("KWinsta::CreateWinstaAndDesktop( kts ):err");
					return (-1);
				}
			}

			if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &this->token))
			{
				kerror("cannot get current process token: " << GetLastError());
				return -1;
			}

			this->session_state->SetStateLogged(curr_user);

			this->console->Write(this->params.login_successfull_message);

			klog("login accepted: [ " << curr_user << " ]");

			// release ip ban
			this->ip_ban->ResetIPBanConnection(this->sock->GetConnectionIP());

			return(ERROR_SUCCESS);

		}
	}

private:
	// =============================================================================
	// wait for shell to start
	// =============================================================================
	void WaitForActiveShell( HANDLE process )
	{
		ktrace_in( );
		ktrace( "KSession::WaitForActiveShell( )" );

		int ret = WaitForInputIdle( process, 5000 );

		if( ret == 0 ) return;

		kerror( "no active shell ret = " << ret );

		if( ret == WAIT_TIMEOUT )
		{
			// kill the process we can't wait for it anymore
			if( !TerminateProcess(process, 0) ) kerror("can't terminate bad shell");
		}

		this->TerminateSession( );
	}

private:
	// =============================================================================
	// start shell
	// =============================================================================
	bool StartShell( )
	{
		ktrace_in( );
		ktrace( "KSession::StartShell( " << this->user << ", " << this->token << " )" );

		if (this->winStaName.empty())
		{
			klog("Error: winStaName is empty");
			return(false);
		}

		KWinsta::LoadUserEnvironment( this->token, this->user );

		STARTUPINFO si;
		SECURITY_ATTRIBUTES sa;

		ZeroMemory( &sa, sizeof( sa ));
		sa.nLength = sizeof( sa );
		sa.bInheritHandle = true;

		ZeroMemory( &si, sizeof( si ) );
		si.cb = sizeof( si );
//		si.lpDesktop = "ktsWINSTA\\ktsDESK";
		std::string deskname = this->winStaName + "\\" + this->DESKTOP_NAME;
		si.lpDesktop = new char[deskname.length() + 1];
		strcpy(si.lpDesktop, deskname.c_str());

		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_SHOW;

		if( this->params.pipe_mode )
		{
			HANDLE writePipe;
			if(!::CreatePipe(&this->shell_out, &writePipe, &sa, 0))
			{
				kerror("CreatePipe:err");
				ExitProcess(0);
			}
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			si.hStdError = writePipe;
			si.hStdOutput = writePipe;
			si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		}

		ZeroMemory( &sa, sizeof( sa ));
		sa.nLength = sizeof( sa );
		sa.bInheritHandle = true;

		BOOL ret = false;

		
		EnterCriticalSection( &this->cs );

		PROCESS_INFORMATION pri = {0};
		if( this->params.debug_flag || !KWinsta::IsLocalSystem())
		{
			klog("create process with current user");
			ret = CreateProcess( NULL, ( char * )this->params.shell_command.c_str( ), NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pri );
//			ret = CreateProcess("C:\\Windows\\system32\\cmd.exe", NULL, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pri);			
		}
		else
		{
			ret = CreateProcessAsUser( this->token, NULL, ( char * )this->params.shell_command.c_str( ), NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pri );
		}
		if( ret )
		{
//			WaitForActiveShell( pri.hProcess );

			this->shell = pri;

			this->session_state->SetStateShell( );

			if(KWinsta::IsLocalSystem())
			{
				this->shell_group->CreateJob( this->shell.dwProcessId );
			}
			else
			{
				this->shell_group->SetParent(this->shell.dwProcessId);
			}

			LeaveCriticalSection( &this->cs );

			this->flags->Raise( "shell" );

			return( true );
		}
		else
		{
			DWORD err = GetLastError();
			LeaveCriticalSection( &this->cs );

			this->flags->Raise( "shell" );

			kerror( "can't start shell (" << err << ")"  );
			return( false );
		}
	}

private:
	// =============================================================================
	// shutdown -> ExitProcess gracefully thread
	// =============================================================================
	static DWORD WINAPI ShutDownThread( LPVOID p )
	{
		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KSession::ShutDownThread( " << p << " )" );

		KSession * term = ( KSession * )p;

		EnterCriticalSection( &term->cs );

		term->session_state->SetStateClosed( );

		KCommUtils comm( term->sock, term->ssh, term->flags, term->is_ssh );
		comm.Close( true );

		klog( "session.exe end" );

		term->shell_group->Terminate( );

		ExitProcess( 0 );

		kend_try_block;
	}

private:
	// =============================================================================
	// try to shut down session gracefully, then terminate it
	// =============================================================================
	void TerminateSession( )
	{
		ktrace_in( );
		ktrace( "KSession::TerminateSession( )" );

		EnterCriticalSection( &this->cs );

		if( this->flags->IsRaised( "terminate" ) )
		{
			LeaveCriticalSection( &this->cs );
			return;
		}
		else
		{
			this->flags->Raise("terminate");
			LeaveCriticalSection( &this->cs );
		}

		CreateThread( NULL, 0, KSession::ShutDownThread, ( LPVOID )this, 0, NULL );
		Sleep( 3000 );
		if( this->session_state->GetState() != STATE_CLOSED )
		{
			kerror("shutdown timeout");
			this->session_state->SetStateClosed( );
		}
		this->shell_group->Terminate( );
		ExitProcess( 0 );
	}

private:
	// =============================================================================
	// health monitor thread
	// =============================================================================
	static DWORD WINAPI HealthMonitorThread( LPVOID p )
	{
		// todo: move console output to separate thread as we might be blocked forever if the console is locked

		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KSession::HealthMonitorThread( " << p << " )" );

		KSession * term = ( KSession * )p;

		while( true )
		{
			Sleep( term->params.health_monitor_timeout );

			if( !KWinsta::ProcessStillActive( term->service ) )
			{
				term->console->Write( term->params.service_stopped_message );

				ktrace_level( 10 );
				klog( "KTS service stopped" );

				break;
			}

			if( term->flags->IsRaised( "shell" ) && !KWinsta::ProcessStillActive( term->shell.hProcess ) )
			{
				term->console->Write( term->params.shell_dead_message );

				ktrace_level( 10 );
				klog( "shell is dead" );

				break;
			}

			if( term->flags->IsRaised( "idle_timeout" ) ) 
			{
				term->console->Write( term->params.idle_timeout_message );

				ktrace_level( 10 );
				klog( "idle timeout" );

				break;
			}

			if( term->flags->IsRaised( "pass_timeout" ) )
			{
				term->console->Write( term->params.pass_timeout_message );

				ktrace_level( 10 );
				klog( "password timeout" );

				break;
			}

			if( term->flags->IsRaised( "pipe_io_timeout" ) )
			{
				ktrace_level( 10 );
				klog( "pipe timeout" );

				break;
			}

			if( term->flags->IsRaised( "terminate" ) ) break;

			if( term->flags->IsRaised( "sock_io_timeout" ) ) 
			{
				ktrace_level( 10 );
				klog( "sock_io_timeout" );

				KCommUtils comm( term->sock, term->ssh, term->flags, term->is_ssh );
				comm.Close( true );

				term->flags->Raise( "disconnect" );
			}

			if( term->flags->IsRaised( "disconnect" ) ) 
			{
				term->flags->Reset( "disconnect" );
				klog( "disconnected" );
			}
		}

		Sleep( 1000 );
		klog( "shutdown" );
		term->TerminateSession();

		return 0;
		kend_try_block;
	}

private:
	// =============================================================================
	// log in thread the ssh way
	// =============================================================================
	static DWORD WINAPI LoginSSHThread( LPVOID p )
	{
		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KSession::LoginSSHThread( " << p << " )" );

		Sleep( 2000 );

		KSession * term = ( KSession * )p;

		std::string pipe_name = "";

		if( term->password_must_change )
		{
			// change the password and terminate the session
			term->ChangeUserPassword( term->ssh->username, term->ssh->password );
			term->TerminateSession( );

			return 0;
		}

		if( term->params.allow_disconnected_sessions ) pipe_name = term->session_state->ChooseDisconnectedSession( term->params.auto_reconnect_session );

		if( pipe_name == "" ) 
		{
			term->StartShell( );
		}
		else
		{
			klog( "proxy mode: " << pipe_name );
			term->pipe_name = pipe_name;
			term->flags->Raise( "proxy" );
		}

		return 0;

		kend_try_block;

	}
private:
	// =============================================================================
	// log in thread the telnet way
	// =============================================================================
	static DWORD WINAPI LoginTelnetThread( LPVOID p )
	{

		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KSession::LoginTelnetThread( " << p << " )" );

		Sleep( 1000 );

		KSession * term = ( KSession * )p;

		term->flags->Enable( "pass_timeout" );

		term->console->Write( term->params.welcome_message );

		unsigned loop = 0;
		while( true )
		{
			DWORD error;
			std::string pass;
			std::string user;
			std::string domain;
			std::string ip = term->sock->GetConnectionIP();

			if (term->auto_logon->GetAutoCredentials(ip, user, pass, domain))
			{
				klog( "auto login found: [ " << ip << " / " << user << "@" << domain << " ]" );
				term->params.default_user = user;
				term->params.default_pass = pass;
				term->params.default_domain = domain;
			}

			if( term->params.default_user == "" )
			{
				term->console->Write( term->params.login_message );
				term->console->SetConsoleMode( ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT );
				term->console->Read( user );
				if( user == "" ) continue;
			}
			else user = term->params.default_user;

			if( term->params.default_pass == "" )
			{
				term->console->SetConsoleMode( ENABLE_LINE_INPUT );
				term->console->Write( term->params.pass_message );
				term->console->Read( pass );
			}
			else pass = term->params.default_pass;

			term->user = user;
			error = term->LogonUser1( user, pass );

			if( error == ERROR_SUCCESS ) 
			{
				if(!term->subsystems->CheckAccess(term->token, "terminal"))
				{
					klog("access denied to subsystem [terminal] for user " << term->user);
					term->TerminateSession( );
				}

				term->console->Write( "\n" );

				std::string pipe_name = "";

				if( term->password_must_change )
				{
					// change the password and terminate the session
					term->ChangeUserPassword( user, pass );
					term->TerminateSession( );

					return 0;
				}

				if( term->params.allow_disconnected_sessions ) pipe_name = term->session_state->ChooseDisconnectedSession( term->params.auto_reconnect_session );

				if( pipe_name == "" ) 
				{
					term->StartShell( );
				}
				else
				{
					term->pipe_name = pipe_name;
					term->flags->Raise( "proxy" );
				}

				break;
			}

			std::string message = KWinsta::GetErrorMessage( error );
			term->console->Write( "\n" + message  + "\n\n" );

			ktrace( "login refused: [ " << user << " ] - " << message );
			klog( "login refused: [ " << user << " ] - " << message );

			++loop;

			if( loop >= term->params.max_login_attempts )
			{
				klog( "max login attempts" );

				term->TerminateSession( );
			}
		}

		term->flags->Disable( "pass_timeout" );
		return 0;

		kend_try_block;
	}

private:
	// =============================================================================
	// log in the telnet way
	// =============================================================================
	void LoginTelnet( )
	{
		ktrace_in( );
		ktrace( "KSession::LoginTelnet( )" );

		CreateThread( NULL, 0, KSession::LoginTelnetThread, ( LPVOID )this, 0, NULL );
	}


private:
	// =============================================================================
	// read the first packet
	// =============================================================================
	void ReadFirstPacket( std::string & str )
	{
		ktrace_in( );
		ktrace( "KSession::ReadFirstPacket( )" );

		unsigned i = 0;

		KCommUtils comm( this->sock, this->ssh, this->flags, this->is_ssh );

		// whait for the first packet
		while( true )
		{
			if( !comm.Receive( str ) )
			{
				klog( "can't read first packet" );

				this->TerminateSession( );
			}

			if( str != "" ) break;

			i++;
			Sleep( 100 );

			if( i * 100 >= this->params.first_packet_timeout ) break;
		}
	}
private:
	// =============================================================================
	// log in the ssh way
	// =============================================================================
	void LoginSSH( )
	{

		ktrace_in( );
		ktrace( "KSession::LoginSSH( )" );

		this->flags->Enable( "pass_timeout" );
		bool pubkey_auth = false;
		unsigned loop = 0;
		while( true )
		{
			if( !this->ssh->Login( ) )
			{
				ktrace_level( 10 );
				klog( "can't read login" );

				this->ssh->LogErrorMessage();
				this->TerminateSession( );
			}

			std::string _user = this->ssh->username;
			std::string pass = this->ssh->password;
			std::string publickey = this->ssh->publickey;
			pubkey_auth = false;

			if(publickey != "" )
			{
				std::string domain;
				if(this->publickey_logon->GetPublickeyCredentials(publickey, _user, pass, domain))
				{
					// we have the user credentials based on his publickey
					if( domain != "" )
					{
						_user = _user + "@" + domain;
					}
					klog("doing publickey authentication");
					pubkey_auth = true;
				}
				else
				{
					klog("doing publickey authentication - unknown publickey");
				}
			}

			DWORD error;

			this->user = _user;
			if (pubkey_auth)
			{
				error = this->LogonUserWithoutPassword(_user);
			}
			else
			{
				error = this->LogonUser1( _user, pass );
			}

			if( error == ERROR_SUCCESS ) 
			{
				// complete login process
				if( !this->ssh->Login( true ) )
				{
					ktrace_level( 10 );
					klog( "can't complete login" );

					this->ssh->LogErrorMessage();
					this->TerminateSession( );
				}
				break;
			}
			std::string message = KWinsta::GetErrorMessage( error );

			ktrace( "login refused: [ " << _user << " ] - " << message );
			klog( "login refused: [ " << _user << " ] - " << message );

			++loop;

			if( loop >= this->params.max_login_attempts )
			{
				klog( "max login attempts" );

				this->TerminateSession( );
			}
		}

		// we are logged at that point
		this->flags->Disable( "pass_timeout" );


		// we must change the password, force ssh session
		if( this->password_must_change )
		{
			if(!this->subsystems->CheckAccess(this->token, "terminal"))
			{
				klog("access denied to subsystem [terminal] for user " << this->user);
				this->TerminateSession( );
			}
			// ssh session
			CreateThread( NULL, 0, KSession::LoginSSHThread, ( LPVOID )this, 0, NULL );
		}
		else
		{
			// read first packet to check for sftp subsystem
			this->ReadFirstPacket( this->ssh_first_read );

			if( this->ssh->IsSftp( this->ssh_first_read ) )
			{
				if(!this->subsystems->CheckAccess(this->token, "file-transfer"))
				{
					klog("access denied to subsystem [file-transfer] for user " << this->user);
					this->TerminateSession( );
				}
				// we enter sftp loop and never return back
				this->SftpLoop();
			}
			else
			{
				if(!this->subsystems->CheckAccess(this->token, "terminal"))
				{
					klog("access denied to subsystem [terminal] for user " << this->user);
					this->TerminateSession();
				}
				// ssh session
				CreateThread( NULL, 0, KSession::LoginSSHThread, ( LPVOID )this, 0, NULL );
			}
		}
	}

private:
	// =============================================================================
	// sftp init
	// =============================================================================
	void RunSftpInitScript()
	{
		ktrace_in( );
		ktrace( "KSession::RunSftpInitScript( )" );

		if(!this->params.sftp_init.empty())
		{
			KWinsta::ExpandEnvironmentString( this->params.sftp_init );
			KWinsta::LoadUserEnvironment( this->token, this->user );
			KWinsta::RunCommandAsUser(this->token, this->params.sftp_init );
		}
	}
			
private:
	// =============================================================================
	// sftp loop
	// =============================================================================
	void SftpLoop()
	{
		ktrace_in( );
		ktrace( "KSession::SftpLoop( )" );

//		if( !this->params.allow_sftp )
//		{
//			klog( "sftp not allowed" );
//			this->TerminateSession( );
//		}

		this->session_state->SetStateSftp();

		klog( "sftp started" );

		// call the sftp_init script, we don't care if this will fail
		this->RunSftpInitScript();

		KSftp::SetUserToken( this->token );

		std::string buff = this->ssh_first_read;

		KWinsta::LoadUserEnvironment( this->token, this->user );

		// go to sftp_root folder, we don't care if this will fail
		KWinsta::ExpandEnvironmentString( this->params.sftp_root );
		klog("Current directory: " << this->params.sftp_root);
		if (!SetCurrentDirectory(this->params.sftp_root.c_str()))
		{
			kerror("Cannot change directory");
		}

		KCommUtils comm( this->sock, this->ssh, this->flags, this->is_ssh );

		// sftp loop
		while( true )
		{
			if( buff != "" )
			{
				std::string buff_out = "";
				int offset = KSftp::Process( buff, buff_out );
				if( offset == -1 ) break;

				if( offset > 0 ) buff = buff.substr( offset );

				if( !comm.Send( buff_out ) ) break;
			}

			std::string tmp;
			if( !comm.Receive( tmp ) ) break;

			buff += tmp;

			if( buff.length() > 100000)
			{
				kerror( "sftp buffer too long" );
				break;
			}

//			Sleep( this->params.refresh_delay );
			Sleep( 1 );
		}

		klog( "sftp end session" );
		this->TerminateSession( );
	}
public:
	// =============================================================================
	// kterm main loop
	// =============================================================================
	void Run( )
	{
		ktrace_in( );
		ktrace_level( 10 );

		kbegin_try_block;

		ktrace( "KSession::Run( )" );

		this->Init();

		ktrace_level( 0 );

		if( this->is_ssh ) this->LoginSSH();
		else this->LoginTelnet();

		this->SocketCommunicationLoop( );

		// if this is port forward request we will never return from this loop
		if( this->is_ssh ) 
		{
			if(!this->subsystems->CheckAccess(this->token, "port-forward"))
			{
				klog("access denied to subsystem [port-forward] for user " << this->user);
				this->TerminateSession( );
			}
			this->PortForwardLoop( );
		}

		if( this->params.allow_disconnected_sessions )
		{
			if( this->flags->IsRaised( "proxy" ) ) this->ProxyCommunicationLoop( );
			else if( this->flags->IsRaised( "shell" ) ) this->PipeCommunicationLoop( );
		}

		klog("shutdown session");
		this->TerminateSession( );
		kend_try_block;
	}

private:
	// =============================================================================
	// port forward loop
	// =============================================================================
	void PortForwardLoop( )
	{
		ktrace_in( );
		ktrace( "KSession::PortForwardLoop( )" );

		KPortForward pf(this->ssh, this->flags, this->iniFileName);

		if( !pf.IsResourceRequest() )
		{
			ktrace("this is not resource request");
			return;
		}

//		if( !this->params.allow_port_forwarding )
//		{
//			klog( "port forwarding not allowed" );
//			this->TerminateSession( );
//		}

		std::string request;

		klog( "begin port forward" );

		this->flags->Disable( "idle_timeout" );

		// we will enter the port forwarding loop and never exit it
		while(true)
		{
			if( pf.IsResourceRequest() )
			{
				if( !pf.GetPortForwardRequest(request) )
				{
					klog( "only direct-tcpip port forward allowed" );
					break;
				}

				pf.CreatePortForwardChannel(request);
			}

			if( !pf.TransferChannelData() ) break;

			Sleep( 1 );
//			Sleep( this->params.refresh_delay );

		}

		klog( "port forward end" );
		this->TerminateSession( );
	}

private:
	// =============================================================================
	// socket communication loop
	// =============================================================================
	void SocketCommunicationLoop( )
	{
		ktrace_in( );
		ktrace( "KSession::SocketCommunicationLoop( )" );

		KCommUtils comm(this->sock, this->ssh, this->flags, this->is_ssh);

		std::string buff;
		buff.reserve( STRING_INIT_SIZE );

		std::string output;
		output.reserve( STRING_INIT_SIZE );

		this->flags->Enable( "idle_timeout" );

		buff = std::string( 1, '\x00' );
		std::string telnet_init;
		this->Consume( buff, telnet_init );

		if( this->is_ssh ) telnet_init = "";

		if( !comm.Send( telnet_init ) ) return;

		while( true )
		{
			buff.clear();
			output.clear();

			if( this->flags->IsRaised( "proxy" ) ) return;

			if( !comm.Send( this->Export( ) ) ) return;

			if( !comm.Receive( buff ) ) return;

			if( buff != "" ) 
			{
				this->flags->Reset("idle_timeout");
				this->flags->Reset("net_check_delay");
				this->Consume( buff, output );
				if( !comm.Send( output ) ) return;
			}
			else if( this->flags->IsRaised( "net_check_delay" ) ) 
			{
				this->flags->Reset("net_check_delay");
				if( !comm.Send( this->telnet->AreYouThere( ) ) ) return;
			}

			this->FixConsoleScreenSize( this->is_ssh );

			Sleep( this->params.refresh_delay );
		}
	}

private:
	// =============================================================================
	// pipe communication loop
	// =============================================================================
	void PipeCommunicationLoop( )
	{
		ktrace_in( );
		ktrace( "KSession::PipeCommunicationLoop( )" );

		KCommUtils comm( this->sock, this->ssh, this->flags, this->is_ssh );
		comm.Close( true );

		this->flags->Raise( "disconnect" );

		this->CreatePipe( );

		this->session_state->SetStateDisconnected( );

		this->AcceptPipe( );

		this->session_state->SetStatePipe( );

		this->flags->Reset("net_check_delay");

		std::string output;
		output.reserve( STRING_INIT_SIZE );

		std::string buff;
		buff.reserve( STRING_INIT_SIZE );

		while( true )
		{
			output.clear( );
			buff.clear( );

			this->WritePipe( this->Export( ) );

			this->ReadPipe( buff );

			if( buff != "" )
			{
				this->flags->Reset("idle_timeout");
				this->flags->Reset("net_check_delay");
				this->Consume( buff, output );

				this->WritePipe( output );
			}
			else if( this->flags->IsRaised( "net_check_delay" ) ) 
			{
				this->flags->Reset("net_check_delay");
				this->WritePipe( this->telnet->AreYouThere( ) );
			}

			this->FixConsoleScreenSize( false );

			Sleep( this->params.refresh_delay );
		}
	}

private:
	// =============================================================================
	// construct telnet naws sequence
	// =============================================================================
	std::string naws( int width, int height )
	{
		ktrace_in( );
		ktrace( "KSession::naws( )" );

		std::string buff = "\xff\xfa\x1f\x01\x01\x01\x01\xff\xf0";

		buff[ 3 ] = ( unsigned char )( width / 256 );
		buff[ 4 ] = ( unsigned char )( width % 256 );
		buff[ 5 ] = ( unsigned char )( height / 256 );
		buff[ 6 ] = ( unsigned char )( height % 256 );

		return buff;
	}

private:
	// =============================================================================
	// proxy communication loop
	// =============================================================================
	void ProxyCommunicationLoop( )
	{
		ktrace_in( );
		ktrace( "KSession::ProxyCommunicationLoop( )" );

		this->session_state->SetStateProxy( );

		KPipe _pipe;
		KCommUtils comm( this->sock, this->ssh, this->flags, this->is_ssh );

		if( !_pipe.Connect( this->pipe_name ) ) 
		{
			klog( "can't connect pipe " << this->pipe_name );
			this->session_state->SetStateZombie( this->pipe_name );
			return;
		}


		int width = 63;
		int height = 21;
		if( !_pipe.Write( naws( width, height ) ) ) goto end_loop;
		if( this->is_ssh )
		{
			this->ssh->ScreenSize();
			width = this->ssh->screenWidth;
			height = this->ssh->screenHeight;
		}
		else
		{
			width = this->telnet->screen.X;
			height = this->telnet->screen.Y;
		}
		if( !_pipe.Write( naws( width, height ) ) ) goto end_loop;


		while( true )
		{
			std::string buff;
			DWORD bytes;

			if( !_pipe.PeekData( &bytes ) ) goto end_loop;
			if( bytes > 0 )
			{
				if( !_pipe.Read( buff ) ) goto end_loop;
				if( !comm.Send( buff ) ) goto end_loop;
			}

			if( !comm.Receive( buff ) ) goto end_loop;
			if( buff != "" )
			{
				if( !_pipe.Write( buff ) ) goto end_loop;
			}

			if( this->is_ssh )
			{
				this->ssh->ScreenSize();
				if( width != this->ssh->screenWidth || height != this->ssh->screenHeight )
				{
					width = this->ssh->screenWidth;
					height = this->ssh->screenHeight;

					if( !_pipe.Write( naws( width, height ) ) ) goto end_loop;
				}
			}

			Sleep( this->params.refresh_delay );

		}
end_loop:
		klog( "disconnecting" );
	}
};