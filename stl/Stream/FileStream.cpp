// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "stl/Stream/FileStream.h"
#include "stl/Algorithms/StringUtils.h"

#ifdef PLATFORM_WINDOWS
#	define fread	_fread_nolock
#	define fwrite	_fwrite_nolock
#	define fflush	_fflush_nolock
#	define fclose	_fclose_nolock
#	define ftell	_ftelli64_nolock
#	define fseek	_fseeki64_nolock
#endif

#if defined(PLATFORM_LINUX)
#	define fseek	fseeko
#	define ftell	ftello
#endif
#if defined(PLATFORM_LINUX) or defined(PLATFORM_ANDROID)
#   define fopen_s( _outFile_, _name_, _mode_ ) (*_outFile_=fopen(_name_, _mode_))
#endif


namespace VSA
{

/*
=================================================
	constructor
=================================================
*/
	FileRStream::FileRStream (StringView filename)
	{
		fopen_s( OUT &_file, filename.data(), "rb" );
		
		if ( _file )
			_fileSize = _GetSize();
		else
			VSA_LOGI( "Can't open file: \""s << filename << '"' );
	}
	
	FileRStream::FileRStream (const char *filename) : FileRStream{ StringView{filename} }
	{}

	FileRStream::FileRStream (const String &filename) : FileRStream{ StringView{filename} }
	{}
	
/*
=================================================
	destructor
=================================================
*/
	FileRStream::~FileRStream ()
	{
		if ( _file )
			fclose( _file );
	}
	
/*
=================================================
	_GetSize
=================================================
*/
	BytesU  FileRStream::_GetSize () const
	{
		ASSERT( IsOpen() );

		const int64_t	curr = ftell( _file );
		CHECK( fseek( _file, 0, SEEK_END ) == 0 );

		const int64_t	size = ftell( _file );
		CHECK( fseek( _file, curr, SEEK_SET ) == 0 );

		return BytesU(uint64_t(size));
	}
	
/*
=================================================
	SeekSet
=================================================
*/
	bool  FileRStream::SeekSet (BytesU pos)
	{
		ASSERT( IsOpen() );

		if ( pos == _position )
			return true;

		_position = Min( pos, _fileSize );

		return (fseek( _file, int64_t(pos), SEEK_SET ) == 0);
	}
	
/*
=================================================
	Read2
=================================================
*/
	BytesU  FileRStream::Read2 (OUT void *buffer, BytesU size)
	{
		ASSERT( IsOpen() );

		BytesU	readn{ fread( buffer, 1, size_t(size), _file )};

		_position += readn;

		return readn;
	}
//-----------------------------------------------------------------------------


	
/*
=================================================
	constructor
=================================================
*/
	FileWStream::FileWStream (StringView filename)
	{
		fopen_s( OUT &_file, filename.data(), "wb" );

		if ( not _file )
			VSA_LOGI( "Can't open file: \""s << filename << '"' );
	}
	
	FileWStream::FileWStream (const char *filename) : FileWStream{ StringView{filename} }
	{}

	FileWStream::FileWStream (const String &filename) : FileWStream{ StringView{filename} }
	{}
	
/*
=================================================
	destructor
=================================================
*/
	FileWStream::~FileWStream ()
	{
		if ( _file )
			fclose( _file );
	}
	
/*
=================================================
	Position
=================================================
*/
	BytesU  FileWStream::Position () const
	{
		ASSERT( IsOpen() );

		return BytesU(uint64_t(ftell( _file )));
	}
	
/*
=================================================
	Size
=================================================
*/
	BytesU  FileWStream::Size () const
	{
		ASSERT( IsOpen() );

		const int64_t	curr = ftell( _file );
		CHECK( fseek( _file, 0, SEEK_END ) == 0 );

		const int64_t	size = ftell( _file );
		CHECK( fseek( _file, curr, SEEK_SET ) == 0 );

		return BytesU(uint64_t(size));
	}
	
/*
=================================================
	Write2
=================================================
*/
	BytesU  FileWStream::Write2 (const void *buffer, BytesU size)
	{
		ASSERT( IsOpen() );

		return BytesU(fwrite( buffer, 1, size_t(size), _file ));
	}
	
/*
=================================================
	Flush
=================================================
*/
	void  FileWStream::Flush ()
	{
		ASSERT( IsOpen() );

		CHECK( fflush( _file ) == 0 );
	}


}	// VSA
