#pragma once
#include "..\shared\KTrace.hxx"
#include "..\shared\KScreen.hxx"
#include "..\shared\KTelnet.hxx"

class KScreenExport
{
public:
	bool dumb_client;

private:
	/*==============================================================================
	 * vars
	 *=============================================================================*/
	KTelnet & telnet;
	COORD & screen;
	KScreen nscreen;
	KScreen oscreen;
	bool mismatch;

public:
	/*==============================================================================
	 * constructor
	 *=============================================================================*/
	KScreenExport( KTelnet & telnet1 ) : telnet( telnet1 ), screen( telnet1.screen )
	{
		ktrace_in( );

		ktrace( "KScreenExport::KScreenExport( ... )" );

		this->mismatch = false;

	}
	// KScreenExport( )

private:
	/*==============================================================================
	 * fix nscreen to reflect client screen
	 *=============================================================================*/
	std::string ClientServerMismatch( KScreen & _nscreen )
	{
		if( this->screen.X - 1 == _nscreen.screen.Right
		&&  this->screen.Y - 1 == _nscreen.screen.Bottom - _nscreen.screen.Top ) return( "." );

		if( _nscreen.screen.Right > this->screen.X - 1 ) 
		{
				this->nscreen.screen.Right = this->screen.X - 1;
				this->nscreen.screen.Left = 0;
		}


		if( _nscreen.cursor.X > this->screen.X - 1 ) _nscreen.cursor.X = 0;
		if( _nscreen.cursor.Y - _nscreen.screen.Top > this->screen.Y - 1 ) _nscreen.cursor.Y = 0;

		if( _nscreen.screen.Bottom - _nscreen.screen.Top > this->screen.Y - 1 )
		{
			if( _nscreen.screen.Top == _nscreen.buff.Top && _nscreen.screen.Bottom == _nscreen.buff.Bottom )
			{
				// no scroll back so fix the screen
				_nscreen.screen.Bottom = _nscreen.screen.Top + this->screen.Y - 1;
				_nscreen.buff.Bottom = _nscreen.screen.Top + this->screen.Y - 1;
			}
		}

		// mismatch
		if( this->screen.X - 1 != this->nscreen.screen.Right - this->nscreen.screen.Left
		||  this->screen.Y - 1 != this->nscreen.screen.Bottom - this->nscreen.screen.Top )
		{

			CONSOLE_SCREEN_BUFFER_INFO csbi;
			KConsole con;
			con.GetConsoleInfo( csbi );
			if( con.HasScroll( csbi ) )
			{
				if( this->mismatch ) return( "" );
				this->mismatch = true;
				return( this->telnet.Cls( KConsole::BACKGROUND_RED | KConsole::FOREGROUND_WHITE | KConsole::FOREGROUND_INTENSITY )
					    + this->telnet.GotoXY( 20, this->screen.Y / 2 )
						+ "KTS error: client screen mismatch" );
			}
		}

		return( "." );
	}

private:
	/*==============================================================================
	 * check if needs flat screen export1
	 *=============================================================================*/
	bool NeedFlatScreen( const KScreen & _nscreen, const KScreen & _oscreen )
	{
		ktrace_in( );

		ktrace( "KScreenExport::NeedFlatScreen( )" );

		// client server mismatch
		if( this->mismatch )
		{
			this->mismatch = false;
			return( true );
		}

		// whole buff
		if( _nscreen.buff.Top == 0 )
		{
			ktrace( "[ true ] - whole buff" );
			return( true );
		}

		// screen size differ
		if( ( _nscreen.screen.Bottom - _nscreen.screen.Top ) != ( _oscreen.screen.Bottom - _oscreen.screen.Top ) )
		{
			ktrace( "[ true ] - screen differ" );
			return( true );
		}

		// screen width differ
		if( ( _nscreen.screen.Right - _nscreen.screen.Left ) != ( _oscreen.screen.Right - _oscreen.screen.Left ) )
		{
			ktrace( "[ true ] - screen width differ" );
			return( true );
		}

		// screen top
		if( _nscreen.screen.Top < _oscreen.screen.Top )
		{
			ktrace( "[ true ] - screen top" );
			return( true );
		}

		KConsole console;
		// scrollback
		if( console.HasScroll( _nscreen.csbi ) != console.HasScroll( _oscreen.csbi ) )
		{
			ktrace( "[ true ] - scrollback differ" );
			return( true );
		}

		ktrace( "[ false ]" );
		return( false );
		
	}
	// bool NeedFlatScreen( const KScreen & nscreen, const KScreen & oscreen )

private:
	/*==============================================================================
	 * get index of tje last visible char of the row
	 *=============================================================================*/
	int GetLastVisible( const KScreen & _nscreen, int row )
	{
		std::wstring blank = L" \t\r\n";
//		blank += std::wstring( 0, 1 );	// http://www.kpym.com/phpbb/viewtopic.php?t=504 : Visual Studio 2008 uses the constructor: basic_string(size_type n, charT c, const allocator_type& alloc = allocator_type());

		int last;
		for( last = _nscreen.screen.Right; last >= 0; last-- )
		{
			if( blank.find( _nscreen.data[ row ][ last ].Char.UnicodeChar ) == std::wstring::npos
			|| _nscreen.data[ row ][ last ].Attributes != _nscreen.csbi.wAttributes ) break;
		}
		return( last );
	}
	// int GetLastVisible( const KScreen & nscreen, int row )

private:
	/*==============================================================================
	 * convert row to oscreen "screen" coords
	 *=============================================================================*/
	int to_oscreen( const KScreen & _nscreen, const KScreen & _oscreen, int row )
	{
		return( _oscreen.screen.Top + ( row - _nscreen.buff.Top ) );
	}

private:
	/*==============================================================================
	 * convert row to nscreen "screen" coords
	 *=============================================================================*/
	SHORT to_nscreen( const KScreen & _nscreen, const KScreen & _oscreen, SHORT row )
	{
		SHORT ret = _nscreen.buff.Top + ( row - _oscreen.buff.Top );
		if( ret > _nscreen.buff.Bottom ) ret = _nscreen.buff.Bottom;
		return( ret );
	}

private:
	/*==============================================================================
	 * check if "chars" differ
	 *=============================================================================*/
	bool char_differ( const CHAR_INFO & nci, const CHAR_INFO & oci )
	{
		return( nci.Char.UnicodeChar != oci.Char.UnicodeChar || nci.Attributes != oci.Attributes );
	}

private:
	/*==============================================================================
	 * get row difference
	 *=============================================================================*/
	std::string DiffRow( const KScreen & _nscreen, const KScreen & _oscreen, int row, int & col )
	{
		ktrace_in( );

		ktrace( "KScreenExport::DiffRow( )"  );

		int oldrow = this->to_oscreen( _nscreen, _oscreen, row );
		int last = _nscreen.screen.Right;

		if( this->dumb_client && row == _nscreen.buff.Bottom ) last--;

		int from = -1;
		int to = -2;
		for( int i = 0; i <= last; i++ )
		{
			if( this->char_differ( _nscreen.data[ row ][ i ], _oscreen.data[ oldrow ][ i ] ) )
			{
				if( from == -1 ) from = i;
				to = i;
			}
		}

		col = from;
		if( oldrow > _oscreen.cursor.Y )
		{
			from = 0;
			col = -1;
		}

		std::string export1;
		for( int i = from; i <= to; i++ )
		{
			// !!!
			export1 += this->telnet.Color( _nscreen.data[ row ][ i ].Attributes );
			export1 += this->telnet.Encode( _nscreen.data[ row ][ i ].Char.UnicodeChar );
		}


		return( export1 );
	}

private:
	/*==============================================================================
	 * get the difference btw screens
	 *=============================================================================*/
	std::string DiffScreen( const KScreen & _nscreen, const KScreen & _oscreen )
	{
		ktrace_in( );
		
		ktrace( "KScreenExport::DiffScreen( )" );

		COORD cursor;
		std::string export1;
		int col;

		cursor.X = _oscreen.cursor.X;
		cursor.Y = this->to_nscreen( _nscreen, _oscreen, _oscreen.cursor.Y );

		// new        old
		// -----      -----
		//  5          
		//  6          4
		// [7  ]      [5  ]
		// [8  ]      [6  ]
		// [9  ]      [7  ]

		// export1 difference
		SHORT i;
		for( i = _nscreen.buff.Top; this->to_oscreen( _nscreen, _oscreen, i ) <= _oscreen.buff.Bottom; i++  )
		{
			std::string tmp = this->DiffRow( _nscreen, _oscreen, i, col );

			if( tmp != "" )
			{
				// we have position
				if( col >= 0 )
				{
					// set posision it differs the oscreen cursor
					if( col != cursor.X || this->to_oscreen( _nscreen, _oscreen, i ) != cursor.Y )
					{
						export1 += this->telnet.GotoXY( col, i - _nscreen.buff.Top );
					}
					export1 += tmp;
					cursor.X = ( SHORT )( col + tmp.length( ) );
				}
				else
				{
					for( int j = cursor.Y; j < i; j++ ) export1 += this->telnet.CrLf( );
					export1 += tmp;
					cursor.X = ( SHORT )tmp.length( );
				}

				cursor.Y = i;
			}
		}

		// export1 flat screen
		export1 += this->FlatScreen( _nscreen, i, _nscreen.buff.Bottom, &cursor );

		// fix cursor pos
		if( _nscreen.cursor.X != cursor.X || _nscreen.cursor.Y != cursor.Y )
		{
			if( _nscreen.cursor.Y - _nscreen.buff.Top >= this->screen.Y && _nscreen.cursor.X == 0 )
			{
				export1 += "\n";
//				for( int i = 0; i <= nscreen.cursor.Y - nscreen.buff.Top - this->screen.Y; i++ ) export1 += "\n";
//				export1 += this->telnet.GotoXY( nscreen.cursor.X, this->screen.Y - 1 );
			}
			else
				export1 += this->telnet.GotoXY( _nscreen.cursor.X, _nscreen.cursor.Y - _nscreen.buff.Top );
		}

		// fix cursor pos on blank screen
		if( export1 == "" )
		{
			if( _nscreen.cursor.X != _oscreen.cursor.X || _nscreen.cursor.Y != _oscreen.cursor.Y )
			{
				export1 += this->telnet.GotoXY( _nscreen.cursor.X, _nscreen.cursor.Y - _nscreen.buff.Top );
			}
		}
		else export1 += this->telnet.GotoXY( _nscreen.cursor.X, _nscreen.cursor.Y - _nscreen.buff.Top );

		return( export1 );
	}

private:
	/*==============================================================================
	 * export1 a flat screen
	 *=============================================================================*/
	std::string FlatScreen( const KScreen & _nscreen, SHORT top = -1, int bottom = -1, COORD * pcursor = 0 )
	{
		ktrace_in( );

		ktrace( "KScreenExport::FlatScreen( )"  );

		std::string export1;
		COORD cursor = { 0, 0 };

		if( top == -1 && bottom == -1 )
		{
			// we export1 the entire screen
			export1 += this->telnet.Cls( _nscreen.csbi.wAttributes );
			top = _nscreen.buff.Top;
			bottom = _nscreen.buff.Bottom;
		}


		cursor.Y = top;
		for( SHORT i = top; i <= bottom; i++ )
		{
			std::string tmp;
			for( int j = 0; j <= this->GetLastVisible( _nscreen, i ); j++ )
			{
				// !!!
				tmp += this->telnet.Color( _nscreen.data[ i ][ j ].Attributes );
				tmp += this->telnet.Encode( _nscreen.data[ i ][ j ].Char.UnicodeChar );
			}

			if( tmp != "" )
			{
				for( int j = cursor.Y; j < i; j++ ) export1 += this->telnet.CrLf( );
				export1 += tmp;
				cursor.X = ( SHORT )tmp.length( );
				cursor.Y = i;
			}
		}

		if( export1 != "" && pcursor != 0 ) *pcursor = cursor;
		if( export1 != "" )
		{
			export1 = this->telnet.CrLf( ) + export1;
			if( pcursor == 0 )
			{
				// fix cursor pos
				if( _nscreen.cursor.X != cursor.X || _nscreen.cursor.Y != cursor.Y )
				{
					if( _nscreen.cursor.Y - _nscreen.buff.Top >= this->screen.Y && _nscreen.cursor.X == 0 )
					{
						export1 += "\n";
					}
					else
						export1 += this->telnet.GotoXY( _nscreen.cursor.X, _nscreen.cursor.Y - _nscreen.buff.Top );
				}
			}
		}

		return( export1 );
	}

public:
	/*==============================================================================
	 * export1 the screen
	 *=============================================================================*/
	std::string Export( const KScreen & _nscreen, const KScreen & _oscreen )
	{
		ktrace_in( );
		ktrace_level( 0 );

		ktrace( "KScreenExport::Export2( )" );

		if( this->NeedFlatScreen( _nscreen, _oscreen ) ) return( this->FlatScreen( _nscreen ) );
		else return( this->DiffScreen( _nscreen, _oscreen ) );

	}

public:
	/*==============================================================================
	 * export1 the screen ( auto refresh )
	 *=============================================================================*/
	std::string Export( )
	{
		ktrace_in( );
		ktrace_level( 0 );
		ktrace( "KScreenExport::Export( )" );

		std::string str;

		this->nscreen.Refresh( );

		str = this->ClientServerMismatch( this->nscreen );
		if( str == "." ) str = this->Export( this->nscreen, this->oscreen );

		this->oscreen = this->nscreen;

		return( str );
	}
};