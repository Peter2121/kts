/*==============================================================================
 * KpyM Telnet/SSH Server
 * Copyright (C) 2002-2008  Kroum Grigorov
 * e-mail:  support@kpym.com
 * web:     http://www.kpym.com
 * license: ( see ../license.txt for details )
 *=============================================================================*/
#define _ktrace
#define NOCRYPT
#include <iostream>
#include "..\shared\kts.h"
#include "KDaemon.hxx"
#include "KSetup.hxx"

void usage()
{
	std::cout << "Usage: daemon.exe [[-debug|-run|-install|-uninstall|-start|-stop|-rsakey|-setup] [-config <inifile>]]" << std::endl;
//	std::cout << "\t-config option is accepted for -debug|-run|-install|-setup commands only and must follow the related command" << std::endl;
}

// =============================================================================
// app entry point
// =============================================================================
void main( int argc, char **argv )
{
	ktrace_in( );
	ktrace_level( 10 );

	kbegin_try_block;

	KWinsta::SetToModuleDirectory( );
	KWinsta::SetKtsHome( );
	bool haveConfigFileName = false;

	if( argc == 1 )
	{
		KDaemon::RunService(KTS_INI_FILE);
		return;
	}

	if( argc >= 2 )
	{
		if (strcmp("-config", argv[1]) == 0)
		{
			if(argc == 3)
			{
				if (strlen(argv[2]) > 2)
					KDaemon::RunService(argv[2]);
			}
			usage();
			return;
		}

		if (argc == 4)
		{
			if (strcmp("-config", argv[2]) == 0)
			{
				if (strlen(argv[3]) > 2)
					haveConfigFileName = true;
			}
		}
		
		if( strcmp( "-debug", argv[ 1 ] ) == 0 )
		{
			if (haveConfigFileName)
			{
				KDaemon::Debug(argv[3]);
				return;
			}
			else
			{
				KDaemon::Debug( );
				return;
			}
		}

		if (strcmp("-run", argv[1]) == 0)
		{
			if (haveConfigFileName)
			{
				KDaemon::Run(argv[3]);
				return;
			}			
			else
			{
				KDaemon::Run();
				return;
			}
		}

		// init trace
		KSetup setup;

		ktrace_in( );
		ktrace_level( 10 );
		ktrace( "setup" );

		if( strcmp( "-install", argv[ 1 ] ) == 0 ) 
		{
			if (haveConfigFileName)
			{
				setup.Install(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}
		if( strcmp( "-uninstall", argv[ 1 ] ) == 0 ) 
		{
			if (haveConfigFileName)
			{
				setup.Uninstall(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}
		if( strcmp( "-start", argv[ 1 ] ) == 0 )
		{
			if (haveConfigFileName)
			{
				setup.Start(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}
		if( strcmp( "-stop", argv[ 1 ] ) == 0 )
		{
			if (haveConfigFileName)
			{
				setup.Stop(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}
		if( strcmp( "-rsakey", argv[ 1 ] ) == 0 )
		{
			if (haveConfigFileName)
			{
				setup.RSAKey(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}
		if( strcmp( "-setup", argv[ 1 ] ) == 0 )
		{
			if (haveConfigFileName)
			{
				setup.Run(argv[3]);
			}
			else
			{
				setup.Install(KTS_INI_FILE);
			}
		}

	}
	return;

	kend_try_block;
}
