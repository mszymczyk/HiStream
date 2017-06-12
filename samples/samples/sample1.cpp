// sample1.cpp : Defines the entry point for the console application.

#include "stdafx.h"
#include "../../src/HiStream.h"
//#include "../../src/HiStreamText.h"
#include "../../src/HiStreamXml.h"
#include <fstream>
#include <vector>

using namespace HiStream;

namespace GraphNodeTags
{
enum Type
{
	invalid,

	//dependencyNode = MakeTag( "depn" ),
	//dagNode        = MakeTag( "dagn" ),
	node1Test		= MakeTag( "nod1" ),
	node2Test		= MakeTag( "nod2" ),
	node3Test		= MakeTag( "nod3" ),

    count,
};
}

namespace AttrTag
{
enum Type
{
    invalid,

	//nodeName             = MakeTag( "ndnm" ),
 //   setAttribute         = MakeTag( "seta" ),
 //   setAttributeObsolete = MakeTag( "aobs" ),
 //   setAttributeNew      = MakeTag( "anew" ),
	U8Test				= MakeTag( "u8te" ),
	S8Test				= MakeTag( "s8te" ),
	U16Test				= MakeTag( "u16t" ),
	S16Test				= MakeTag( "s16t" ),
	U32Test             = MakeTag( "u32t" ),
	S32Test             = MakeTag( "s32t" ),
	U64Test				= MakeTag( "u64t" ),
	S64Test				= MakeTag( "s64t" ),
	FloatTest			= MakeTag( "flot" ),
	DoubleTest			= MakeTag( "dobt" ),
	StringTest			= MakeTag( "strt" ),

	U8ArrayTest			= MakeTag( "u8ar" ),
	S8ArrayTest			= MakeTag( "s8ar" ),
	U16ArrayTest		= MakeTag( "u16a" ),
	S16ArrayTest		= MakeTag( "s16a" ),
	U32ArrayTest		= MakeTag( "u32a" ),
	S32ArrayTest		= MakeTag( "s32a" ),
	U64ArrayTest		= MakeTag( "u64a" ),
	S64ArrayTest		= MakeTag( "s64a" ),
	FloatArrayTest		= MakeTag( "farr" ),
	DoubleArrayTest		= MakeTag( "darr" ),

	StringU8Test		= MakeTag( "su8t" ),
	StringS8Test		= MakeTag( "ss8t" ),
	StringU16Test		= MakeTag( "su16" ),
	StringS16Test		= MakeTag( "ss16" ),
	StringU32Test		= MakeTag( "su32" ),
	StringS32Test		= MakeTag( "ss32" ),
	StringU64Test		= MakeTag( "su64" ),
	StringS64Test		= MakeTag( "ss64" ),
	StringFloatTest		= MakeTag( "sflo" ),
	StringDoubleTest	= MakeTag( "sdob" ),

	dataWithLayout		= MakeTag( "dawl" ),
	dataTest			= MakeTag( "data" ),

    count,
};
}


template<typename T, size_t bufferSize>
void fillArray( T (&buffer)[bufferSize], T initValue )
{
	for ( size_t i = 0; i < bufferSize; ++i )
		buffer[i] = initValue + static_cast<T>( i );
}


#define ADD_ARRAY(typeName, typeNameUC, initValue ) \
{ \
typeName arr[8]; \
fillArray( arr, (typeName)initValue ); \
os.add##typeNameUC##Array( AttrTag::typeNameUC##Test, arr, sizeof( arr ) / sizeof( arr[0] ) ); \
}


//template<typename T>
//void fillArray( T buffer[], size_t bufferSize, T initValue )
//{
//	for ( size_t i = 0; i < bufferSize; ++i )
//		buffer[i] = initValue + (i % std::numeric_limits<T>::max());
//}

void testWrite( OutputStream& os )
{
	//u32 tag = MakeTag2( "depn" );
	//char tagBuf[5];
	//TagToStr( tag, tagBuf );

	os.begin();

	//os.pushChild( GraphNodeTags::dependencyNode );

	//float arr[4] = { 1, 2, 3, 4 };
	//char arr[5] = { 'h', 'e', 'l', 'l', 'o' };
	//u32 arr[2] = { 0xdeadbeef, 0xbaadc0de };
	//os.setData( DataType::custom, arr, sizeof( arr ) );

	//os.addString( AttrTag::nodeName, "node1" );
	//os.addStringFloat( AttrTag::setAttribute, "attr0", 0.0f );
	//os.addStringFloat( AttrTag::setAttributeObsolete, "attr1", 1.0f );
	//os.addStringFloat( AttrTag::setAttributeNew, "attr2", 2.0f );

	//{
	//	u64 arr[64];
	//	for ( u64 i = 0; i < 64; ++i )
	//		arr[i] = 0xffffffff + i + 1;

	//	os.addU64Array( AttrTag::u64Test, arr, 64 );
	//}

	//os.popChild();



	//os.pushChild( GraphNodeTags::dependencyNode );

	//float arrf[4] = { 1, 2, 3.147897676f, 4 };

	////os.setData( DataType::floatArray, arrf, sizeof( arrf ) );
	//os.addFloatArray( AttrTag::floatArray, arrf, 4 );

	//os.addString( AttrTag::nodeName, "node1" );
	//os.addStringFloat( AttrTag::setAttribute, "attr3", 3.0f );
	//os.addStringFloat( AttrTag::setAttributeNew, "attr4", 4.0f );
	//os.addStringFloat( AttrTag::setAttributeObsolete, "attr5", 5.0f );

	//os.popChild();

	os.pushChild( GraphNodeTags::node1Test );

		os.addU8( AttrTag::U8Test, 11 );
		os.addS8( AttrTag::S8Test, -11 );
		os.addU16( AttrTag::U16Test, 2222 );
		os.addS16( AttrTag::S16Test, -2222 );
		os.addU32( AttrTag::U32Test, 333333 );
		os.addS32( AttrTag::S32Test, -333333 );
		os.addU64( AttrTag::U64Test, 0xdeadbeef00 );
		os.addS64( AttrTag::S64Test, 0xFFFFFF1000000000 );
		os.addFloat( AttrTag::FloatTest, 99.5f );
		os.addDouble( AttrTag::DoubleTest, 109.5 );
		os.addString( AttrTag::StringTest, "sampleString" );

	os.popChild();



	os.pushChild( GraphNodeTags::node2Test );

	ADD_ARRAY( u8,  U8,  1 );
	ADD_ARRAY( s8,  S8,  2 );
	ADD_ARRAY( u16, U16, 3 );
	ADD_ARRAY( s16, S16, 4 );

	ADD_ARRAY( u32, U32, 5 );
	ADD_ARRAY( s32, S32, 6 );
	ADD_ARRAY( u64, U64, 7 );
	ADD_ARRAY( s64, S64, 8 );

	ADD_ARRAY( float, Float, 3.14f );
	ADD_ARRAY( double, Double, 99.5 );

	os.pushChild( GraphNodeTags::node3Test );

	os.addStringU8( AttrTag::StringU8Test, "u8", 8 );
	os.addStringS8( AttrTag::StringS8Test, "s8", -8 );

	os.addStringU16( AttrTag::StringU16Test, "u16", 16 );
	os.addStringS16( AttrTag::StringS16Test, "s16", -16 );

	os.addStringU32( AttrTag::StringU32Test, "u32", 32 );
	os.addStringS32( AttrTag::StringS32Test, "s32", -32 );
	os.addStringU64( AttrTag::StringU64Test, "u64", 64 );
	os.addStringS64( AttrTag::StringS64Test, "s64", -64 );

	os.addStringFloat( AttrTag::StringFloatTest, "float", 3.14f );
	os.addStringDouble( AttrTag::StringDoubleTest, "double", -95.5f );

	struct DE
	{
		u64 b;
		s64 bs;
		double d;
		float f[2];
		u16 s;
		s16 ss;
		u32 c;
		s32 cs;
		u8 a[2];
		s8 e[2];
	};

	DE deArr[2] = {
		  { 1000, -10000, 3.14, { 1.5f, 2.3f }, 3, -4, 0xbaadc0de, -16, { 'a', 'b' }, { 'c', 'd' } }
		, { 1000, -10000, 3.14, { 1.5f, 2.3f }, 3, -4, 0xbaadc0de, -16, { 'a', 'b' }, { 'c', 'd' } }
	};

	//os.setData( { {DataElementType::Float, 2}, {DataElementType::S16, 2}, {DataElementType::U32, 1} }, deArr, sizeof( deArr ) );
	os.addDataWithLayout( AttrTag::dataWithLayout,
	{
		  { DataType::U64, 1 }
		, { DataType::S64, 1 }
		, { DataType::Double, 1 }
		, { DataType::Float, 2 }
		, { DataType::U16, 1 }
		, { DataType::S16, 1 }
		, { DataType::U32, 1 }
		, { DataType::S32, 1 }
		, { DataType::U8, 2 }
		, { DataType::S8, 2 }
	}
		, deArr, sizeof( deArr ), 16
	);

	os.addData( AttrTag::dataTest, deArr, sizeof(deArr), 16 );

	os.popChild();

	os.popChild();

	os.end();
}

int main()
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

	OutputStream os;
	testWrite( os );

	HiStreamBuffer text = convertHisToXml( os.buffer(), os.bufferSize() );

	{
		std::ofstream f( "histream.xml" );
		f.write( text.text(), text.textSize() );
		f.close();
	}

	{
		std::ifstream f( "histream.xml", std::ios::binary );

		size_t fsize = (size_t)f.tellg();
		f.seekg( 0, std::ios::end );
		fsize = (size_t)f.tellg() - fsize;

		std::vector<char> buf( fsize + 1 );
		f.seekg( 0, std::ios::beg );
		f.read( &buf[0], fsize );
		buf[fsize] = '\0';

		f.close();

		HiStreamBuffer bin = convertXmlToHis( &buf[0], buf.size() );
	}

    return 0;
}

