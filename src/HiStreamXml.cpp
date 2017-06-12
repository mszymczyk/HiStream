#include "HiStreamXml.h"
#include "..\3rdParty\pugixml\src\pugixml.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <utility>

namespace HiStream
{

struct ConvertContext
{
	ConvertContext()
	{	}

	Allocator alloc_;
	Logger log_;

	XmlConvertError::Type error_ = XmlConvertError::noError;
	Error::Type osError_ = Error::noError;
	char errorText_[256] = {};

	void warning( const char* format, ... )
	{
		va_list	args;

		va_start( args, format );
		char buffer[sizeof( errorText_ )];
		/*int nWritten =*/ vsnprintf( buffer, sizeof( buffer ) - 1, format, args );
		buffer[sizeof( buffer ) - 1] = '\0';
		va_end( args );

		if ( log_.logWarning_ )
			log_.logWarning_( buffer );
	}

	void error( XmlConvertError::Type error, const char* format, ... )
	{
		va_list	args;

		va_start( args, format );
		char buffer[sizeof( errorText_ )];
		/*int nWritten =*/ vsnprintf( buffer, sizeof(buffer)-1, format, args );
		buffer[sizeof( errorText_ ) - 1] = '\0';
		va_end( args );

		if ( log_.logError_ )
			log_.logError_( buffer );

		if ( error_ == XmlConvertError::noError )
		{
			error_ = error;
			memcpy( errorText_, buffer, sizeof( errorText_ ) );
		}
	}

	void osError( Error::Type error, const char* format, ... )
	{
		va_list	args;

		va_start( args, format );
		char buffer[sizeof( errorText_ )];
		/*int nWritten =*/ vsnprintf( buffer, sizeof( buffer ) - 1, format, args );
		buffer[sizeof( errorText_ ) - 1] = '\0';
		va_end( args );

		if ( log_.logError_ )
			log_.logError_( buffer );

		osError_ = error;

		if ( error_ == XmlConvertError::noError )
		{
			error_ = XmlConvertError::outputStreamError;
			memcpy( errorText_, buffer, sizeof( errorText_ ) );
		}
	}
};


// very awkward implementation of memory stream that can grow
class strstream
{
public:
	strstream( const Allocator& alloc )
		: alloc_( alloc )
	{	}

	~strstream()
	{
		if ( buf_ )
			alloc_.free_( buf_, alloc_.userPtr_ );
	}

	void write( const char* str, size_t strLen )
	{
		size_t newSize = nWritten_ + strLen + 1; // alloc one for '\0'

		if ( usingStaticBuffer_ )
		{
			if ( newSize > eMaxStaticChars )
			{
				usingStaticBuffer_ = false;
				nCapacity_ = newSize + 256;
				buf_ = reinterpret_cast<char*>( alloc_.alloc_( nCapacity_, 1, alloc_.userPtr_ ) );
				memcpy( &buf_[0], staticBuffer_, nWritten_ );
				memcpy( &buf_[nWritten_], str, strLen );
				nWritten_ += strLen;
				buf_[nWritten_] = '\0';
			}
			else
			{
				// we're dealing here with short strings, use simple for
				for ( size_t i = 0; i < strLen; ++i )
					staticBuffer_[nWritten_ + i] = str[i];
				nWritten_ += strLen;
				staticBuffer_[nWritten_] = '\0';
			}
		}
		else
		{
			if ( newSize > nCapacity_ )
			{
				size_t inc = nCapacity_ * 2;
				if ( inc > 8 * 1024 * 1024 )
					inc = 8 * 1024 * 1024;

				size_t newCapacity = nCapacity_ + inc;
				if ( newCapacity < newSize )
					newCapacity = newSize;

				char* newBuf = reinterpret_cast<char*>( alloc_.alloc_( newCapacity, 1, alloc_.userPtr_ ) );
				if ( buf_ )
				{
					memcpy( newBuf, buf_, nWritten_ );
					alloc_.free_( buf_, alloc_.userPtr_ );
				}
				buf_ = newBuf;
			}

			memcpy( &buf_[nWritten_], str, strLen );
			nWritten_ += strLen;
			buf_[nWritten_] = '\0';
		}
	}

	void write( const char* str )
	{
		size_t strLen = strlen( str );
		write( str, strLen );
	}

	void write( char c )
	{
		write( &c, 1 );
	}

	void write( u64 val )
	{
		char buf[32];
		int len = snprintf( buf, 32, "%llu", val );
		if ( len < 32 )
			write( buf, len );
		else
			HISTREAM_ASSERT( false );
	}
	
	void write( s64 val )
	{
		char buf[64];
		int len = snprintf( buf, 32, "%lld", val );
		if ( len < 64 )
			write( buf, len );
		else
			HISTREAM_ASSERT( false );
	}

	void write( double val )
	{
		char buf[64];
		int len = snprintf( buf, 32, "%g", val );
		if ( len < 64 )
			write( buf, len );
		else
			HISTREAM_ASSERT( false );
	}

	const char* c_str() const
	{
		if ( usingStaticBuffer_ )
			return staticBuffer_;
		else
			return &buf_[0];
	}

	size_t length() const
	{
		return nWritten_;
	}

private:
	Allocator alloc_;
	char* buf_ = nullptr;
	enum { eMaxStaticChars = 32 };
	char staticBuffer_[eMaxStaticChars];
	size_t nCapacity_ = 0;
	size_t nWritten_ = 0;
	bool usingStaticBuffer_ = true;
};

#define errorExpectedAttribute(attrName) "attr: expected '%s' attribute. (node=%s, attr=%s)", attrName, xmlNodeTag, tagStr
#define errorAttributeOutOfRange(attrName) "attr: value '%s' is out of range. (node=%s, attr=%s)", attrName, xmlNodeTag, tagStr
#define errorIncorrectAlignment "attr: incorrect alignment. (node=%s, attr=%s)", xmlNodeTag, tagStr
#define errorValue "attr: value error (empty, length mismatch or out of range). (node=%s, attr=%s)", xmlNodeTag, tagStr
#define errorDataLayoutDelimiter "attr: expected delimiter: ';'. (node=%s, attr=%s)", xmlNodeTag, tagStr



#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


#define INTEGER_TO_XML(typeName, typeNameUC) \
case HiStream::AttributeType::typeNameUC: \
{ \
const typeName val = a.get##typeNameUC(); \
attr.append_attribute( STRINGIFY(typeName) ).set_value( val ); \
break; \
}


#define FLOAT_TO_XML(attrName, typeName, typeNameUC) \
case HiStream::AttributeType::typeNameUC: \
{ \
const typeName val = a.get##typeNameUC(); \
attr.append_attribute( STRINGIFY(attrName) ).set_value( val ); \
break; \
}


#define UNSIGNED_ARRAY_TO_XML(typeName, typeNameUC) \
case HiStream::AttributeType::typeNameUC##Array: \
{ \
const typeName* arr = a.typeName##Array(); \
strstream ss(ctx.alloc_); \
for ( u32 i = 0; i < arrSize; ++i ) \
{ \
	if ( i > 0 ) \
		ss.write( " ", 1 ); \
	ss.write( (u64)arr[i] ); \
} \
attr.append_attribute( STRINGIFY(typeName) ).set_value( ss.c_str() ); \
break; \
}


#define SIGNED_ARRAY_TO_XML(typeName, typeNameUC) \
case HiStream::AttributeType::typeNameUC##Array: \
{ \
const typeName* arr = a.typeName##Array(); \
strstream ss(ctx.alloc_); \
for ( u32 i = 0; i < arrSize; ++i ) \
{ \
	if ( i > 0 ) \
		ss.write( " ", 1 ); \
	ss.write( (s64)arr[i] ); \
} \
attr.append_attribute( STRINGIFY(typeName) ).set_value( ss.c_str() ); \
break; \
}


#define FLOAT_ARRAY_TO_XML(attrName, typeName, typeNameUC) \
case HiStream::AttributeType::typeNameUC##Array: \
{ \
const typeName* arr = a.typeName##Array(); \
strstream ss(ctx.alloc_); \
for ( u32 i = 0; i < arrSize; ++i ) \
{ \
	if ( i > 0 ) \
		ss.write( " ", 1 ); \
	ss.write( (double)arr[i] ); \
} \
attr.append_attribute( STRINGIFY(attrName) ).set_value( ss.c_str() ); \
break; \
}


#define STRING_NUMBER_TO_XML(attrType, attrName, attrNameUC) \
case HiStream::AttributeType::String##attrNameUC: \
{ \
attrType val; \
size_t strLen = 0; \
const char* str = a.string##attrNameUC( strLen, val ); \
attr.append_attribute( "s" ).set_value( str ); \
attr.append_attribute( STRINGIFY(attrName) ).set_value( val ); \
break; \
}


#define STRING_NUMBER_TO_XML2(attrType, attrNameUC) STRING_NUMBER_TO_XML( attrType, attrType, attrNameUC )



static Allocator* g_pugixml_alloc;

// Memory allocation function interface; returns pointer to allocated memory or NULL on failure
void* pugixml_allocation_function( size_t size )
{
	return g_pugixml_alloc->alloc_( size, 16, g_pugixml_alloc->userPtr_ );
}

// Memory deallocation function interface
void pugixml_deallocation_function( void* ptr )
{
	g_pugixml_alloc->free_( ptr, g_pugixml_alloc->userPtr_ );
}


void convertBinNodeToXmlNode( const ConvertContext& ctx, const Node& node, pugi::xml_node& xmlNode )
{
	char tagBuffer[5];
	xmlNode.append_attribute( "tag" ).set_value( TagToStr(node.tag(), tagBuffer) );
	xmlNode.append_attribute( "numNode" ).set_value( node.numChildren() );
	xmlNode.append_attribute( "numAttr" ).set_value( node.numAttributes() );

	for ( const Attribute& a : node.attributes() )
	{
		AttributeType::Type at = a.type();

		pugi::xml_node attr = xmlNode.append_child( "attr" );
		attr.append_attribute( "tag" ).set_value( TagToStr( a.tag(), tagBuffer ) );
		attr.append_attribute( "type" ).set_value( AttributeType::ToString( at ) );
		u32 arrSize = 0;
		if (    ( at >= AttributeType::U8Array && at <= AttributeType::DoubleArray )
			 || ( at == AttributeType::String )
			 || ( at >= AttributeType::StringU8 && at <= AttributeType::StringDouble )
			 )
		{
			arrSize = a.arrayLength();
			attr.append_attribute( "len" ).set_value( arrSize );
		}

		switch ( at )
		{
		INTEGER_TO_XML( u8, U8 )
		INTEGER_TO_XML( s8, S8 )

		INTEGER_TO_XML( u16, U16 )
		INTEGER_TO_XML( s16, S16 )

		INTEGER_TO_XML( u32, U32 )
		INTEGER_TO_XML( s32, S32 )

		INTEGER_TO_XML( u64, U64 )
		INTEGER_TO_XML( s64, S64 )

		FLOAT_TO_XML( f, float, Float )
		FLOAT_TO_XML( d, double, Double )

		case HiStream::AttributeType::String:
		{
			size_t strLen = 0;
			const char* str = a.getString( strLen );
			attr.append_attribute( "s" ).set_value( str );
			break;
		}

		UNSIGNED_ARRAY_TO_XML( u8, U8 );
		SIGNED_ARRAY_TO_XML( s8, S8 );

		UNSIGNED_ARRAY_TO_XML( u16, U16 );
		SIGNED_ARRAY_TO_XML( s16, S16 );

		UNSIGNED_ARRAY_TO_XML( u32, U32 );
		SIGNED_ARRAY_TO_XML( s32, S32 );

		UNSIGNED_ARRAY_TO_XML( u64, U64 );
		SIGNED_ARRAY_TO_XML( s64, S64 );

		FLOAT_ARRAY_TO_XML( f, float, Float );
		FLOAT_ARRAY_TO_XML( d, double, Double );


		STRING_NUMBER_TO_XML2( u8, U8 );
		STRING_NUMBER_TO_XML2( s8, S8 );

		STRING_NUMBER_TO_XML2( u16, U16 );
		STRING_NUMBER_TO_XML2( s16, S16 );

		STRING_NUMBER_TO_XML2( u32, U32 );
		STRING_NUMBER_TO_XML2( s32, S32 );

		STRING_NUMBER_TO_XML2( u64, U64 );
		STRING_NUMBER_TO_XML2( s64, S64 );

		STRING_NUMBER_TO_XML( float, f, Float );
		STRING_NUMBER_TO_XML( double, d, Double );


		case HiStream::AttributeType::Data:
		{
			u32 dataSize = a.dataSize();
			attr.append_attribute( "dataSize" ).set_value( dataSize );
			attr.append_attribute( "dataAlign" ).set_value( a.dataAlignment() );

			const u8* src = reinterpret_cast<const u8*>( a.data() );

			strstream ss( ctx.alloc_ );
			for ( u32 i = 0; i < dataSize; ++i )
			{
				u8 v = src[i];
				u8 h = v >> 4;
				u8 l = v & 0xf;
				ss.write( (char)( ( h < 10 ) ? '0' + h : 'a' + h - 10 ) );
				ss.write( (char)( ( l < 10 ) ? '0' + l : 'a' + l - 10 ) );
			}

			attr.append_attribute( "data" ).set_value( ss.c_str() );

			break;
		}
		case HiStream::AttributeType::DataWithLayout:
		{
			const DataLayoutElement* elements = a.dataLayout();
			size_t nLayout = a.dataLayoutCount();

			u32 elementSize = 0;
			for ( size_t ie = 0; ie < nLayout; ++ie )
			{
				elementSize += DataType::SizeInBytes( elements[ie].type ) * elements[ie].nWords;

				pugi::xml_node xmlLayoutElem = attr.append_child( "layout" );
				xmlLayoutElem.append_attribute( "type" ).set_value( DataType::ToString( elements[ie].type ) );
				xmlLayoutElem.append_attribute( "count" ).set_value( elements[ie].nWords );
			}

			u32 dataSize = a.dataSize();

			strstream ss( ctx.alloc_ );

			u32 n = dataSize / elementSize;
			HISTREAM_ASSERT( n * elementSize == dataSize );
			const u8* src = reinterpret_cast<const u8*>( a.data() );
			for ( u32 i = 0; i < n; ++i )
			{
				for ( size_t ie = 0; ie < nLayout; ++ie )
				{
					switch ( elements[ie].type )
					{
					case DataType::U8:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( u8 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							u8 v = *src;
							ss.write( (u64)v );
							ss.write( ' ' );
							src += 1;
						}
						break;
					}
					case DataType::S8:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( s8 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							s8 v = *reinterpret_cast<const s8*>( src );
							ss.write( (s64)v );
							ss.write( ' ' );
							src += 1;
						}
						break;
					}


					case DataType::U16:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( u16 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							u16 v = *reinterpret_cast<const u16*>( src );
							ss.write( (u64)v );
							ss.write( ' ' );
							src += sizeof( u16 );
						}
						break;
					}
					case DataType::S16:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( s16 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							s16 v = *reinterpret_cast<const s16*>( src );
							ss.write( (s64)v );
							ss.write( ' ' );
							src += sizeof( s16 );
						}
						break;
					}


					case DataType::U32:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( u32 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							u32 v = *reinterpret_cast<const u32*>( src );
							ss.write( (u64)v );
							ss.write( ' ' );
							src += sizeof( u32 );
						}
						break;
					}
					case DataType::S32:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( s32 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							s32 v = *reinterpret_cast<const s32*>( src );
							ss.write( (s64)v );
							ss.write( ' ' );
							src += sizeof( s32 );
						}
						break;
					}
					
					
					case DataType::U64:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( u64 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							u64 v = *reinterpret_cast<const u64*>( src );
							ss.write( (u64)v );
							ss.write( ' ' );
							src += sizeof( u64 );
						}
						break;
					}
					case DataType::S64:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( s64 ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							s64 v = *reinterpret_cast<const s64*>( src );
							ss.write( (s64)v );
							ss.write( ' ' );
							src += sizeof( s64 );
						}
						break;
					}


					case DataType::Float:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( float ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							float v = *reinterpret_cast<const float*>( src );
							ss.write( v );
							ss.write( ' ' );
							src += sizeof( float );
						}
						break;
					}
					case DataType::Double:
					{
						HISTREAM_ASSERT( _private::validateAlign( src, alignof( double ) ) );
						for ( size_t j = 0; j < elements[ie].nWords; ++j )
						{
							double v = *reinterpret_cast<const double*>( src );
							ss.write( v );
							ss.write( ' ' );
							src += sizeof( double );
						}
						break;
					}


					default:
						HISTREAM_ASSERT( false );
						break;
					}
				}

				if ( i + 1 < n )
					ss.write( "; ", 2 );
			}

			attr.append_attribute( "numLayout" ).set_value( nLayout );
			attr.append_attribute( "numElements" ).set_value( n );
			attr.append_attribute( "dataSize" ).set_value( dataSize );
			attr.append_attribute( "dataAlign" ).set_value( a.dataAlignment() );
			attr.append_attribute( "data" ).set_value( ss.c_str() );

			break;
		}
		default:
			break;
		}
	}

	for ( const Node& child : node.children() )
	{
		pugi::xml_node xmlChild = xmlNode.append_child( "node" );
		convertBinNodeToXmlNode( ctx, child, xmlChild );
	}
}

struct xml_string_writer : pugi::xml_writer
{
	xml_string_writer( const Allocator& alloc )
		: result_( alloc )
	{	}

	strstream result_;

	void write( const void* data, size_t size )
	{
		result_.write( static_cast<const char*>( data ), size );
	}
};

HiStreamBuffer convertHisToXml( const u8* binData, const size_t binDataSize, Allocator* alloc /*= nullptr*/, Logger* log /*= nullptr*/ )
{
	InputStreamHeader ish( binData, binDataSize );

	InputStream is( binData, binDataSize );

	Node root = is.getRoot();

	ConvertContext ctx;
	if ( alloc )
	{
		ctx.alloc_ = *alloc;
	}
	else
	{
		ctx.alloc_.alloc_ = _private::default_memmory_alloc_func;
		ctx.alloc_.free_ = _private::default_memmory_free_func;
	}

	if ( log )
		ctx.log_ = *log;

	xml_string_writer writer( ctx.alloc_ );

	g_pugixml_alloc = &ctx.alloc_;

	// explicit block is required here, we want doc object to be destroyed before we set g_pugixml_alloc to nullptr
	{
		pugi::set_memory_management_functions( pugixml_allocation_function, pugixml_deallocation_function );

		pugi::xml_document doc;
		pugi::xml_node xmlRoot = doc.append_child( "histr10" );
		xmlRoot.append_attribute( "magic" ).set_value( ish.getMagic() );
		xmlRoot.append_attribute( "binSize" ).set_value( binDataSize );

		convertBinNodeToXmlNode( ctx, root, xmlRoot );

		doc.save( writer );

		// no way to restore default pugixml allocator 
	}

	g_pugixml_alloc = nullptr;

	size_t siz = writer.result_.length();
	char* buf = reinterpret_cast<char*>( ctx.alloc_.alloc_( siz + 1, 64, ctx.alloc_.userPtr_ ) );
	memcpy( buf, writer.result_.c_str(), siz + 1 );

	return HiStreamBuffer( reinterpret_cast<u8*>(buf), siz, ctx.alloc_, ctx.error_, ctx.osError_ );
}

bool extract_float_array( const char*& s, float* dst, size_t nDst )
{
	size_t nRead = 0;
	bool readError = false;

	while ( *s && nRead < nDst )
	{
		char* e = nullptr;
		float v = strtof( s, &e );
		dst[nRead] = v;
		++nRead;

		if ( errno == ERANGE )
		{
			errno = 0;
			s += 1;
			readError = true;
		}

		if ( s == e )
		{
			s += 1;
			readError = true;
		}
		else
			s = e;
	}

	HISTREAM_ASSERT( nRead == nDst );
	return nRead == nDst && !readError;
}

bool extract_double_array( const char*& s, double* dst, size_t nDst )
{
	size_t nRead = 0;
	bool readError = false;

	while ( *s && nRead < nDst )
	{
		char* e = nullptr;
		double v = strtod( s, &e );
		dst[nRead] = v;
		++nRead;

		if ( errno == ERANGE )
		{
			errno = 0;
			s += 1;
			readError = true;
		}

		if ( s == e )
		{
			s += 1;
			readError = true;
		}
		else
			s = e;
	}

	HISTREAM_ASSERT( nRead == nDst );
	return nRead == nDst && !readError;
}

template<typename T>
bool extract_unsigned_array( const char*& s, T* dst, size_t nDst )
{
	size_t nRead = 0;
	bool outOfRange = false;
	bool readError = false;

	while ( *s && nRead < nDst )
	{
		char* e = nullptr;
		unsigned long long int v = strtoull( s, &e, 10 );
		if ( v > std::numeric_limits<T>::max() )
			outOfRange = true;

		dst[nRead] = static_cast<T>( v );
		++nRead;

		if ( errno == ERANGE )
		{
			errno = 0;
			s += 1;
			readError = true;
		}

		if ( s == e )
		{
			s += 1;
			readError = true;
		}
		else
			s = e;
	}

	HISTREAM_ASSERT( nRead == nDst );
	return nRead == nDst && !outOfRange && !readError;
}


template<typename T>
bool extract_signed_array( const char*& s, T* dst, size_t nDst )
{
	size_t nRead = 0;
	bool outOfRange = false;
	bool readError = false;

	while ( *s && nRead < nDst )
	{
		char* e = nullptr;
		long long int v = strtoll( s, &e, 10 );
		if ( v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max() )
			outOfRange = true;

		dst[nRead] = static_cast<T>( v );
		++nRead;

		if ( errno == ERANGE )
		{
			errno = 0;
			s += 1;
			readError = true;
		}

		if ( s == e )
		{
			s += 1;
			readError = true;
		}
		else
			s = e;
	}

	HISTREAM_ASSERT( nRead == nDst );
	return nRead == nDst && !outOfRange && !readError;
}


#define GET_XML_ATTR( attrName ) \
pugi::xml_attribute attr_##attrName = attr.attribute( TOSTRING(attrName) ); \
if ( attr_##attrName.empty() ) \
{ \
	ctx.error( XmlConvertError::typeError, errorExpectedAttribute( TOSTRING(attrName) ) ); \
	continue; \
}


#define GET_XML_ATTR_STRING( attrName, varName ) \
pugi::xml_attribute attr_##attrName = attr.attribute( TOSTRING(attrName) ); \
if ( attr_##attrName.empty() ) \
{ \
	ctx.error( XmlConvertError::typeError, errorExpectedAttribute( TOSTRING(attrName) ) ); \
	continue; \
} \
const char* varName = attr_##attrName.as_string(); \


#define GET_XML_ATTR_UINT( attrName, varName ) \
pugi::xml_attribute attr_##attrName = attr.attribute( TOSTRING(attrName) ); \
if ( attr_##attrName.empty() ) \
{ \
	ctx.error( XmlConvertError::typeError, errorExpectedAttribute( TOSTRING(attrName) ) ); \
	return; \
} \
u32 varName = attr_##attrName.as_uint();



#define GET_XML_ATTR_U64( attrType, varName ) \
pugi::xml_attribute attr_##attrType = attr.attribute( TOSTRING(attrType) ); \
if ( attr_##attrType.empty() ) \
{ \
	ctx.error( XmlConvertError::typeError, errorExpectedAttribute( TOSTRING(attrType) ) ); \
	continue; \
} \
u64 varName##U64 = attr_##attrType.as_ullong(); \
if ( varName##U64 > std::numeric_limits<attrType>::max() ) \
{ \
	ctx.error( XmlConvertError::valueError, errorAttributeOutOfRange( TOSTRING(attrType) ) ); \
} \
attrType varName = static_cast<attrType>( varName##U64 );



#define GET_XML_ATTR_S64( attrType, varName ) \
pugi::xml_attribute attr_##attrType = attr.attribute( TOSTRING(attrType) ); \
if ( attr_##attrType.empty() ) \
{ \
	ctx.error( XmlConvertError::typeError, errorExpectedAttribute( TOSTRING(attrType) ) ); \
	continue; \
} \
s64 varName##S64 = attr_##attrType.as_llong(); \
if ( varName##S64 < std::numeric_limits<attrType>::min() || varName##S64 > std::numeric_limits<attrType>::max() ) \
{ \
	ctx.error( XmlConvertError::valueError, errorAttributeOutOfRange( TOSTRING(attrType) ) ); \
	continue; \
} \
attrType varName = static_cast<attrType>( varName##S64 );



#define GET_XML_ATTR_FLOAT( attrName, varName ) \
GET_XML_ATTR( attrName ) \
float varName = attr_##attrName.as_float();


#define GET_XML_ATTR_DOUBLE( attrName, varName ) \
GET_XML_ATTR( attrName ) \
double varName = attr_##attrName.as_double();



#define CHECK_OUTPUT_STREAM_ERROR \
HISTREAM_ASSERT( os.error() == Error::noError ); \
if ( os.error() ) \
{ \
	ctx.osError( os.error(), "attr: OutpuStream error '%s'. (node=%s, attr=%s)", os.errorStr(), xmlNodeTag, tagStr ); \
	return; \
}

#define CHECK_READ_ARRAY( func ) \
if ( !(func) ) \
{ \
	ctx.error( XmlConvertError::valueError, "attr: array read error. (node=%s, attr=%s)", xmlNodeTag, tagStr ); \
	continue; \
}


#define XML_TO_UNSIGNED( typeName, typeNameUC ) \
case HiStream::AttributeType::typeNameUC: \
{ \
GET_XML_ATTR_U64( typeName, src ); \
os.add##typeNameUC( tag, src ); \
CHECK_OUTPUT_STREAM_ERROR; \
break; \
}


#define XML_TO_SIGNED( typeName, typeNameUC ) \
case HiStream::AttributeType::typeNameUC: \
{ \
GET_XML_ATTR_S64( typeName, src ); \
os.add##typeNameUC( tag, src ); \
CHECK_OUTPUT_STREAM_ERROR; \
break; \
}


#define XML_TO_FLOAT( attrName, typeName, typeNameUC ) \
case HiStream::AttributeType::typeNameUC: \
{ \
	GET_XML_ATTR_FLOAT( attrName, src ); \
	os.add##typeNameUC( tag, src ); \
	CHECK_OUTPUT_STREAM_ERROR; \
	break; \
}


#define XML_TO_UNSIGNED_ARRAY( typeName, funcName ) \
case HiStream::AttributeType::funcName##Array: \
{ \
GET_XML_ATTR_STRING( typeName, src ); \
typeName* dst = os.add##funcName##Array( tag, nullptr, len ); \
CHECK_OUTPUT_STREAM_ERROR; \
CHECK_READ_ARRAY( extract_unsigned_array( src, dst, len ) ); \
break; \
}


#define XML_TO_SIGNED_ARRAY( typeName, funcName ) \
case HiStream::AttributeType::funcName##Array: \
{ \
GET_XML_ATTR_STRING( typeName, src ); \
typeName* dst = os.add##funcName##Array( tag, nullptr, len ); \
CHECK_OUTPUT_STREAM_ERROR; \
CHECK_READ_ARRAY( extract_signed_array( src, dst, len ) ); \
break; \
}


#define XML_TO_FLOAT_ARRAY( attrName, attrType, attrTypeUC ) \
case HiStream::AttributeType::attrTypeUC##Array: \
{ \
GET_XML_ATTR_STRING( attrName, src ); \
attrType* dst = os.add##attrTypeUC##Array( tag, nullptr, len ); \
CHECK_OUTPUT_STREAM_ERROR; \
CHECK_READ_ARRAY( extract_##attrType##_array( src, dst, len ) ); \
break; \
}


#define XML_TO_STRING_UNSIGNED( typeName, typeNameUC ) \
case HiStream::AttributeType::String##typeNameUC: \
{ \
	GET_XML_ATTR_STRING( s, str ); \
	GET_XML_ATTR_U64( typeName, typeNameUC ); \
	os.addString##typeNameUC( tag, str, len, typeNameUC ); \
	CHECK_OUTPUT_STREAM_ERROR; \
	break; \
}


#define XML_TO_STRING_SIGNED( typeName, typeNameUC ) \
case HiStream::AttributeType::String##typeNameUC: \
{ \
	GET_XML_ATTR_STRING( s, str ); \
	GET_XML_ATTR_S64( typeName, typeNameUC ); \
	os.addString##typeNameUC( tag, str, len, typeNameUC ); \
	CHECK_OUTPUT_STREAM_ERROR; \
	break; \
}


#define XML_TO_STRING_FLOAT( attrName, typeName, typeNameUC ) \
case HiStream::AttributeType::String##typeNameUC: \
{ \
	GET_XML_ATTR_STRING( s, str ); \
	GET_XML_ATTR( attrName ) \
	typeName typeNameUC = attr_##attrName.as_##typeName(); \
	os.addString##typeNameUC( tag, str, len, typeNameUC ); \
	CHECK_OUTPUT_STREAM_ERROR; \
	break; \
}


void convertXmlNodeToBinNode( ConvertContext& ctx, const pugi::xml_node& xmlNode, const char* xmlNodeTag, OutputStream& os )
{
	for ( pugi::xml_node attr : xmlNode.children( "attr" ) )
	{
		const char* tagStr = attr.attribute( "tag" ).as_string();
		if ( !tagStr )
		{
			ctx.error( XmlConvertError::tagError, "attr: expected 'tag' attribute. (node=%s)", xmlNodeTag );
			continue;
		}
		TagType tag = MakeTag2( tagStr );

		const char* typeStr = attr.attribute( "type" ).as_string();
		if ( !typeStr )
		{
			ctx.error( XmlConvertError::typeError, errorExpectedAttribute("type") );
			continue;
		}
		AttributeType::Type at = AttributeType::FromString( typeStr );
		if ( at == AttributeType::Invalid )
		{
			ctx.error( XmlConvertError::typeError, "attr: unsupported attribute type '%s'. (node=%s, attr=%s)", typeStr, xmlNodeTag, tagStr );
			continue;
		}

		u32 len = 0;
		if (   ( at >= AttributeType::U8Array && at <= AttributeType::DoubleArray)
			|| ( at == AttributeType::String )
			|| ( at >= AttributeType::StringU8 && at <= AttributeType::StringDouble )
			 )
		{
			pugi::xml_attribute attr_len = attr.attribute( "len" );
			if ( attr_len.empty() )
			{
				ctx.error( XmlConvertError::typeError, errorExpectedAttribute("len") );
				continue;
			}
			len = attr_len.as_uint( 0 );
		}

		switch ( at )
		{
		XML_TO_UNSIGNED( u8, U8 );
		XML_TO_SIGNED( s8, S8 );

		XML_TO_UNSIGNED( u16, U16 );
		XML_TO_SIGNED( s16, S16 );

		XML_TO_UNSIGNED( u32, U32 );
		XML_TO_SIGNED( s32, S32 );

		XML_TO_UNSIGNED( u64, U64 );
		XML_TO_SIGNED( s64, S64 );

		XML_TO_FLOAT( f, float, Float )
		XML_TO_FLOAT( d, double, Double )

		case HiStream::AttributeType::String:
		{
			GET_XML_ATTR_STRING( s, str )
			if ( strlen( str ) != len )
				ctx.error( XmlConvertError::valueError, errorValue );
			os.addString( tag, str, len );
			break;
		}

		XML_TO_UNSIGNED_ARRAY( u8, U8 )
		XML_TO_SIGNED_ARRAY( s8, S8 )

		XML_TO_UNSIGNED_ARRAY( u16, U16 )
		XML_TO_SIGNED_ARRAY( s16, S16 )

		XML_TO_UNSIGNED_ARRAY( u32, U32 )
		XML_TO_SIGNED_ARRAY( s32, S32 )

		XML_TO_UNSIGNED_ARRAY( u64, U64 )
		XML_TO_SIGNED_ARRAY( s64, S64 )

		XML_TO_FLOAT_ARRAY( f, float, Float )
		XML_TO_FLOAT_ARRAY( d, double, Double )


		XML_TO_STRING_UNSIGNED( u8, U8 )
		XML_TO_STRING_SIGNED( s8, S8 )

		XML_TO_STRING_UNSIGNED( u16, U16 )
		XML_TO_STRING_SIGNED( s16, S16 )

		XML_TO_STRING_UNSIGNED( u32, U32 )
		XML_TO_STRING_SIGNED( s32, S32 )

		XML_TO_STRING_UNSIGNED( u64, U64 )
		XML_TO_STRING_SIGNED( s64, S64 )

		XML_TO_STRING_FLOAT( f, float, Float )
		XML_TO_STRING_FLOAT( d, double, Double )


		case HiStream::AttributeType::Data:
		{
			GET_XML_ATTR_UINT( dataSize, dataSize );
			GET_XML_ATTR_UINT( dataAlign, dataAlign );
			u8* dst = reinterpret_cast<u8*>( os.addData( tag, nullptr, dataSize, dataAlign ) );
			CHECK_OUTPUT_STREAM_ERROR;

			GET_XML_ATTR_STRING( data, src );
			u8* d = dst;
			const char* s = src;
			for ( u32 i = 0; i < dataSize; ++i )
			{
				auto toNum = [&]( char c )
				{
					if ( c >= '0' && c <= '9' )
						c -= '0';
					else if ( c >= 'A' && c <= 'F' )
						c -= 'A' - 10;
					else if ( c >= 'a' && c <= 'f' )
						c -= 'a' - 10;
					else
						c = 0;

					return c;
				};

				*d = toNum( s[0] ) * 16 + toNum( s[1] );
				d += 1;
				s += 2;
			}

			break;
		}


		case HiStream::AttributeType::DataWithLayout:
		{
			DataLayoutElement dataLayout[255];
			size_t nLayout = 0;
			for ( pugi::xml_node layout : attr.children( "layout" ) )
			{
				if ( nLayout >= 255 )
				{
					ctx.error( XmlConvertError::layoutError, "attr: max layouts is 255. (node=%s, attr=%s)", xmlNodeTag, tagStr );
					break;
				}

				const char* type = layout.attribute( "type" ).as_string();
				dataLayout[nLayout].type = DataType::FromString( type );
				dataLayout[nLayout].nWords = static_cast<u8>( layout.attribute( "count" ).as_uint() );
				++nLayout;
			}

			if ( ctx.error_ )
				// error logged in above loop
				continue;

			if ( !nLayout )
			{
				ctx.error( XmlConvertError::layoutError, "attr: at least one layout required. (node=%s, attr=%s)", xmlNodeTag, tagStr );
				continue;
			}

			GET_XML_ATTR_UINT( dataSize, dataSize );
			GET_XML_ATTR_UINT( dataAlign, dataAlign );

			void* dataPtr = os.addDataWithLayout( tag, dataLayout, nLayout, nullptr, dataSize, dataAlign );
			CHECK_OUTPUT_STREAM_ERROR;
			u8* dst = reinterpret_cast<u8*>( dataPtr );

			GET_XML_ATTR_STRING( data, data );
			const char* src = data;
			GET_XML_ATTR_UINT( numElements, n );

			bool continueLoop = true;
			for ( u32 i = 0; i < n && continueLoop; ++i )
			{
				for ( size_t il = 0; il < nLayout && continueLoop; ++il )
				{
					size_t count = dataLayout[il].nWords;

					switch ( dataLayout[il].type )
					{
					case DataType::U8:
					{
						if ( !extract_unsigned_array( src, dst, count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof(u8);
						break;
					}
					case DataType::S8:
					{
						if ( !extract_signed_array( src, dst, count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof(s8);
						break;
					}


					case DataType::U16:
					{
						if ( !_private::validateAlign( dst, alignof( u16 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_unsigned_array( src, reinterpret_cast<u16*>(dst), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof(u16);
						break;
					}
					case DataType::S16:
					{
						if ( !_private::validateAlign( dst, alignof( s16 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_signed_array( src, reinterpret_cast<s16*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( s16 );
						break;
					}


					case DataType::U32:
					{
						if ( !_private::validateAlign( dst, alignof( u32 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_unsigned_array( src, reinterpret_cast<u32*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( u32 );
						break;
					}
					case DataType::S32:
					{
						if ( !_private::validateAlign( dst, alignof( s32 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_signed_array( src, reinterpret_cast<s32*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( s32 );
						break;
					}


					case DataType::U64:
					{
						if ( !_private::validateAlign( dst, alignof( u64 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_unsigned_array( src, reinterpret_cast<u64*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( u64 );
						break;
					}
					case DataType::S64:
					{
						if ( !_private::validateAlign( dst, alignof( s64 ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}
						if ( !extract_signed_array( src, reinterpret_cast<s64*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( s64 );
						break;
					}


					case DataType::Float:
					{
						if ( !_private::validateAlign( dst, alignof( float ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}

						if ( !extract_float_array( src, reinterpret_cast<float*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( float );
						break;
					}
					case DataType::Double:
					{
						if ( !_private::validateAlign( dst, alignof( double ) ) )
						{
							ctx.error( XmlConvertError::alignError, errorIncorrectAlignment );
							continueLoop = false;
							continue;
						}

						if ( !extract_double_array( src, reinterpret_cast<double*>( dst ), count ) )
							ctx.error( XmlConvertError::valueError, errorValue );
						dst += count * sizeof( double );
						break;
					}

					default:
						HISTREAM_ASSERT( false );
						break;
					}

				}

				if ( i + 1 < n )
				{
					while ( *src && src[0] != ';' )
						src += 1; // skip ';'

					if ( !src[0] )
					{
						continueLoop = false;
						ctx.error( XmlConvertError::valueError, errorDataLayoutDelimiter );
					}
					else
					{
						src += 1;
					}
				}
			}

			break;
		}
		default:
		{
			ctx.error( XmlConvertError::typeError, "attr: unsupported type '%s'. (node=%s, attr=%s)", typeStr, xmlNodeTag, tagStr );
			break;
		}
		} // switch
	}

	for ( pugi::xml_node node : xmlNode.children( "node" ) )
	{
		const char* tagStr = node.attribute( "tag" ).as_string();
		HISTREAM_ASSERT( tagStr );
		TagType tag = MakeTag2( tagStr );

		os.pushChild( tag );

		convertXmlNodeToBinNode( ctx, node, tagStr, os );

		os.popChild();
	}

}

HiStream::HiStreamBuffer convertXmlToHis( const char* text, size_t textSize, Allocator* alloc /*= nullptr*/, Logger* log /*= nullptr*/ )
{
	ConvertContext ctx;
	if ( alloc )
	{
		ctx.alloc_ = *alloc;
	}
	else
	{
		ctx.alloc_.alloc_ = _private::default_memmory_alloc_func;
		ctx.alloc_.free_ = _private::default_memmory_free_func;
	}

	if ( log )
		ctx.log_ = *log;

	OutputStream os;

	g_pugixml_alloc = &ctx.alloc_;

	// explicit block is required here, we want doc object to be destroyed before we set g_pugixml_alloc to nullptr
	{
		pugi::set_memory_management_functions( pugixml_allocation_function, pugixml_deallocation_function );

		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_buffer( text, textSize );
		if ( !res )
		{
			ctx.error( XmlConvertError::xmlOpenError, "Couldn't load xml file" );
		}
		else
		{
			pugi::xml_node root = doc.root().first_child();
			const u32 binSize = root.attribute( "binSize" ).as_uint();

			os.begin();

			convertXmlNodeToBinNode( ctx, root, "histr10", os );

			os.end();

			if ( os.bufferSize() != binSize )
				ctx.warning( "Bin size mismatch. Original bin size: %u bytes, after conversion: %u bytes", binSize, os.bufferSize() );
		}

		// no way to restore default pugixml allocator 
	}

	g_pugixml_alloc = nullptr;

	if ( ctx.error_ == XmlConvertError::xmlOpenError )
	{
		return HiStreamBuffer( nullptr, 0, ctx.alloc_, ctx.error_, ctx.osError_ );
	}
	else
	{
		u8* buf = os.stealBuffer();
		size_t bufSize = os.bufferSize();
		return HiStreamBuffer( buf, bufSize, ctx.alloc_, ctx.error_, ctx.osError_ );
	}
}

HiStreamBuffer::~HiStreamBuffer()
{
	alloc_.free_( data_, alloc_.userPtr_ );
}

//template<typename T>
//void swap( T& t1, T& t2 )
//{
//	T temp = std::move( t1 ); // or T temp(std::move(t1));
//	t1 = std::move( t2 );
//	t2 = std::move( temp );
//}

HiStreamBuffer::HiStreamBuffer( HiStreamBuffer&& other ) noexcept
{
	std::swap( other.data_, data_ );
	std::swap( other.dataSize_, dataSize_ );
	std::swap( other.alloc_, alloc_ );
	std::swap( other.convertError_, convertError_ );
	std::swap( other.hisError_, hisError_ );
}

HiStreamBuffer& HiStreamBuffer::operator=( HiStreamBuffer&& other ) noexcept
{
	std::swap( other.data_, data_ );
	std::swap( other.dataSize_, dataSize_ );
	std::swap( other.alloc_, alloc_ );
	std::swap( other.convertError_, convertError_ );
	std::swap( other.hisError_, hisError_ );
	return *this;
}

HiStreamBuffer::HiStreamBuffer( u8* bin, size_t binSize, Allocator& alloc, XmlConvertError::Type convertError, Error::Type hisError )
	: data_( bin )
	, dataSize_( binSize )
	, alloc_( alloc )
	, convertError_( convertError )
	, hisError_( hisError )
{	}

} // namespace HiStream
