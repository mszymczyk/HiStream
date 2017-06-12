// hisconv.cpp : Defines the entry point for the console application.
#include "../src/HiStream.h"
#include "../src/HiStreamXml.h"
#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>

//const char* fileExtension( const char* filename )
//{
//	const size_t filenameLen = strlen( filename );
//	if ( filenameLen <= 4 )
//		return nullptr;
//
//	for ( size_t i = filenameLen; i-- > 0; )
//	{
//		if ( filename[i] == '.' )
//			return filename + i;
//	}
//
//	return nullptr;
//}

#define memAlloc(size, align) _aligned_malloc(size, align)
#define memFree(ptr) _aligned_free(ptr)

enum class ConvDir
{
	unknown,
	xmlToHis,
	hisToXml
};

void changeExtension( std::string& filename, const std::string& newExt )
{
	std::string::size_type extPos = filename.rfind( "." );
	if ( extPos != std::string::npos )
		filename.replace( extPos, std::string::npos, newExt );
	else
		filename.append( newExt );
}

int isXml( const std::string& filename )
{
	std::string::size_type extPos = filename.rfind( ".xml" );
	if ( extPos != std::string::npos )
		return 1;
	else
		return 0;
}

int isHis( const std::string& filename )
{
	std::string::size_type extPos = filename.rfind( ".his" );
	if ( extPos != std::string::npos && extPos + 4 == filename.length() )
		return 1;

	// try opening file, exit if it's not .his
	std::ifstream f( filename.c_str(), std::ifstream::binary );
	if ( !f )
	{
		std::cerr << "Couldn't open '" << filename << "'" << std::endl;
		return -1;
	}

	const size_t headerSize = sizeof( HiStream::InputStreamHeader );
	char header[headerSize];

	f.read( header, headerSize );
	if ( !f )
	{
		std::cerr << "Couldn't read histream header'" << filename << "'" << std::endl;
		return -1;
	}

	HiStream::InputStreamHeader hh( reinterpret_cast<const uint8_t*>(header), headerSize );
	if ( strcmp( hh.getMagic(), "histream" ) )
		return 0;

	return 1;
}

bool readFile( const char* filename, uint8_t*& buf, size_t& bufSize )
{
	std::ifstream f( filename, std::ios::binary );

	size_t fsize = (size_t)f.tellg();
	f.seekg( 0, std::ios::end );
	fsize = (size_t)f.tellg() - fsize;

	buf = reinterpret_cast<uint8_t*>( memAlloc( fsize + 1, 64 ) );
	if ( !buf )
		return false;

	f.seekg( 0, std::ios::beg );
	f.read( reinterpret_cast<char*>(buf), fsize );
	if ( !f )
	{
		memFree( buf );
		return false;
	}

	buf[fsize] = '\0';
	bufSize = fsize;

	return true;
}


int main( int argc, char* argv[] )
{
#ifdef _DEBUG
	int flag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	flag |= _CRTDBG_LEAK_CHECK_DF;
	//flag &= ~_CRTDBG_LEAK_CHECK_DF;
	flag |= _CRTDBG_ALLOC_MEM_DF;
	//flag |= _CRTDBG_CHECK_ALWAYS_DF;
	flag &= ~_CRTDBG_CHECK_ALWAYS_DF;
	_CrtSetDbgFlag( flag );
#endif

	if ( argc < 2 )
	{
		std::cerr << "Expected at least one arg: histream or xml file" << std::endl;
		return -1;
	}

	std::string srcFile;
	std::string dstFile;
	ConvDir convDir = ConvDir::unknown;

	if ( argc == 2 )
	{
		srcFile = argv[1];
		if ( isXml( srcFile ) > 0 )
		{
			convDir = ConvDir::xmlToHis;
			dstFile = srcFile;
			changeExtension( dstFile, ".his" );
		}
		
		if ( convDir == ConvDir::unknown )
		{
			int res = isHis( srcFile );
			if ( res > 0 )
			{
				convDir = ConvDir::hisToXml;
				dstFile = srcFile;
				changeExtension( dstFile, ".xml" );
			}
		}

		if ( convDir == ConvDir::unknown )
		{
			std::cerr << "Unknown source file '" << srcFile << "'. Expected histream or xml" << std::endl;
			return -1;
		}
	}
	else if ( argc >= 3 )
	{
		srcFile = argv[1];
		dstFile = argv[2];

		// most common scenario, bin to xml for introspection
		bool srcIsHis = isHis( srcFile ) > 0;
		bool dstIsXml = isXml( dstFile ) > 0;
		if ( srcIsHis && dstIsXml )
		{
			convDir = ConvDir::hisToXml;
		}
		else
		{
			bool srcIsXml = isXml( srcFile ) > 0;
			bool dstIsHis = isHis( dstFile ) > 0;

			if ( srcIsXml && dstIsHis )
			{
				convDir = ConvDir::xmlToHis;
			}
			else
			{
				std::cerr << "Incorrect input/output pair. Expected histream-xml or xml-histream" << std::endl;
				return -1;
			}
		}
	}

	std::cout << srcFile << " -> " << dstFile << std::endl;

	uint8_t* srcFileBuf = nullptr;
	size_t srcFileSize = 0;
	if ( !readFile( srcFile.c_str(), srcFileBuf, srcFileSize ) )
	{
		std::cerr << "Couldn't read source file'" << srcFile << "'" << std::endl;
		return -1;
	}

	if ( convDir == ConvDir::hisToXml )
	{
		//HiStream::HiStreamTextBuffer text = HiStream::convertBinToXml( srcFileBuf, srcFileSize );

		memFree( srcFileBuf );
		srcFileBuf = nullptr;
		srcFileSize = 0;

		//std::ofstream f( "histream.xml" );
		//f.write( text.getText(), text.getTextSize() );
		//f.close();
	}
	else if ( convDir == ConvDir::xmlToHis )
	{
		//HiStream::HiStreamBuffer bin = HiStream::convertXmlToBin( reinterpret_cast<const char*>( srcFileBuf ), srcFileSize );

		memFree( srcFileBuf );
		srcFileBuf = nullptr;
		srcFileSize = 0;

		//std::ofstream f( "histream.xml", std::ofstream::binary );
		//f.write( reinterpret_cast<const char*>( bin.getData() ), bin.getDataSize() );
		//f.close();
	}

	std::cout << "DONE: " << srcFile << " -> " << dstFile << std::endl;

    return 0;
}

