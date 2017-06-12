#pragma once

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <limits>

#define HISTREAM_ASSERT(x) assert(x)

namespace HiStream
{

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;

typedef uint32_t TagType;

// helpers for creating tags from strings

template<size_t N>
constexpr TagType MakeTag( const char (&str)[N] )
{
	static_assert( N == 5, "tag must be 4 characters long" );
	return ( ( str[0] << 24 ) | ( str[1] << 16 ) | ( str[2] << 8 ) | str[3] );
}

inline TagType MakeTag2( const char* str )
{
	return ( ( str[0] << 24 ) | ( str[1] << 16 ) | ( str[2] << 8 ) | str[3] );
}

template<size_t N>
const char* TagToStr( TagType tag, char( &str )[N] )
{
	static_assert( N == 5, "tag buffer must be 5 characters long" );
	str[0] = tag >> 24;
	str[1] = ( tag >> 16 ) & 0xff;
	str[2] = ( tag >> 8 ) & 0xff;
	str[3] = ( tag ) & 0xff;
	str[4] = '\0';
	return str;
}

typedef u32 NodeOffsetType;
typedef u16 AttributeOffsetType;

namespace AttributeType
{
enum Type : unsigned char
{
	Invalid,

	// basic types
	U8,
	S8,
	U16,
	S16,
	U32,
	S32,
	U64,
	S64,
	Float,
	Double,
	String,

	// basic arrays
	U8Array,
	S8Array,
	U16Array,
	S16Array,
	U32Array,
	S32Array,
	U64Array,
	S64Array,
	FloatArray,
	DoubleArray,
	//StringArray,

	// string + basic type
	StringU8,
	StringS8,
	StringU16,
	StringS16,
	StringU32,
	StringS32,
	StringU64,
	StringS64,
	StringFloat,
	StringDouble,

	Data,
	DataWithLayout,

	count
};

const char* ToString( Type type );
Type FromString( const char* str );

} // namespace AttributeType


namespace DataType
{
enum Type : u8
{
	Invalid,
	U8,
	S8,
	U16,
	S16,
	U32,
	S32,
	U64,
	S64,
	Float,
	Double,
	count
};

const char* ToString( Type type );
Type FromString( const char* str );
u32 SizeInBytes( Type type );

} // namespace DataElementType

struct DataLayoutElement
{
	DataType::Type type;
	u8 nWords;
};


namespace Error
{
enum Type : unsigned char
{
	noError,
	noMem,
	dataOverflow, // attribute/node size exceeded
	attrChildOrder, // can't add attribute after adding children (must add all attributes first)
	hierarchyOverflow,
	hierarchyCorrupted, // push/pop don't match
	badLayout, // zero, too many layout elements (255 max) or layout size/alignment is incorrect
	tagOverflow, // too many tags
	badAlign, // output stream is incorrectly aligned
	count
};

const char* ToString( Type type );

}; // namespace Error

// Memory allocation function interface; returns pointer to allocated memory or nullptr on failure
// Returned memory chunk must be aligned on 'alignment' boundary
typedef void* ( *memory_alloc_func )( size_t size, size_t alignment, void* userPtr );

// Memory deallocation function interface
typedef void ( *memory_free_func )( void* ptr, void* userPtr );

typedef void ( *log_func )( const char* msg );


struct Allocator
{
	memory_alloc_func alloc_ = nullptr;
	memory_free_func free_ = nullptr;
	void* userPtr_ = nullptr;
};

struct Logger
{
	log_func logWarning_ = nullptr;
	log_func logError_ = nullptr;
};

#include "HiStream_private.h"

// can be placed on stack
// doesn't allocate any heap memory until begin is called
class OutputStream
{
public:
	OutputStream( const Allocator* alloc = nullptr, const Logger* log = nullptr );
	~OutputStream();

	// returns first error that occurred while writing stream
	Error::Type error() const;
	const char* errorStr() const;

	// must be called to begin stream writing
	void begin();
	// must be called to finalize stream
	void end();

	const u8* buffer() const;
	size_t bufferSize() const;
	// take ownership of internal buffer
	// use proper allocator to free memory ( alloc() for instance )
	u8* stealBuffer();
	Allocator alloc() const;

	// adds child to current node and sets it as write target
	// all attributes must be added before call to pushChild
	void pushChild( TagType tag );
	// pops node and sets parent as write target
	void popChild();

	// base types
	void addU8    ( TagType tag, u8 x );
	void addS8    ( TagType tag, s8 x );
	void addU16   ( TagType tag, u16 x );
	void addS16   ( TagType tag, s16 x );
	void addU32   ( TagType tag, u32 x );
	void addS32   ( TagType tag, s32 x );
	void addU64   ( TagType tag, u64 x );
	void addS64   ( TagType tag, s64 x );
	void addFloat ( TagType tag, float x );
	void addDouble( TagType tag, double x );
	void addString( TagType tag, const char* str, size_t strLen );
	void addString( TagType tag, const char* str );

	// base arrays

	// returns pointer to allocated array
	// one may set arr to nullptr to just initialize memory block and fill it later
	// returned address is naturally aligned for each type

	u8*  addU8Array ( TagType tag, const u8*  arr, u32 num );
	s8*  addS8Array ( TagType tag, const s8*  arr, u32 num );
	u16* addU16Array( TagType tag, const u16* arr, u32 num );
	s16* addS16Array( TagType tag, const s16* arr, u32 num );
	u32* addU32Array( TagType tag, const u32* arr, u32 num );
	s32* addS32Array( TagType tag, const s32* arr, u32 num );
	u64* addU64Array( TagType tag, const u64* arr, u32 num );
	s64* addS64Array( TagType tag, const s64* arr, u32 num );
	float*  addFloatArray ( TagType tag, const float* arr, u32 num );
	double* addDoubleArray( TagType tag, const double* arr, u32 num );

	//void addStringArray( TagType tag, const char* str[], const size_t strLen[], u32 num );

	// string + base type
	// string + base type is common pattern, by having it as a one attribute we save couple of bytes
	// (don't need to store metadata for two attributes)

	void addStringU8 ( TagType tag, const char* str, size_t strLen, u8 x );
	void addStringS8 ( TagType tag, const char* str, size_t strLen, s8 x );
	void addStringU16( TagType tag, const char* str, size_t strLen, u16 x );
	void addStringS16( TagType tag, const char* str, size_t strLen, s16 x );
	void addStringU32( TagType tag, const char* str, size_t strLen, u32 x );
	void addStringS32( TagType tag, const char* str, size_t strLen, s32 x );
	void addStringU64( TagType tag, const char* str, size_t strLen, u64 x );
	void addStringS64( TagType tag, const char* str, size_t strLen, s64 x );
	void addStringFloat ( TagType tag, const char* str, size_t strLen, float x );
	void addStringDouble( TagType tag, const char* str, size_t strLen, double x );

	void addStringU8 ( TagType tag, const char* str, u8 x );
	void addStringS8 ( TagType tag, const char* str, s8 x );
	void addStringU16( TagType tag, const char* str, u16 x );
	void addStringS16( TagType tag, const char* str, s16 x );
	void addStringU32( TagType tag, const char* str, u32 x );
	void addStringS32( TagType tag, const char* str, s32 x );
	void addStringU64( TagType tag, const char* str, u64 x );
	void addStringS64( TagType tag, const char* str, s64 x );
	void addStringFloat ( TagType tag, const char* str, float x );
	void addStringDouble( TagType tag, const char* str, double x );

	// prefer addDataWithLayout when possible
	// this is used mostly for convenience, when providing data layout is troublesome
	void* addData( TagType tag, const void* data, u32 dataSize, u32 alignment );
	// alignment must match DataLayoutElement::type alignment
	// max number of layout elements is 255
	void* addDataWithLayout( TagType tag, const DataLayoutElement* layout, size_t nLayout, const void* data, u32 dataSize, u32 alignment );
	void* addDataWithLayout( TagType tag, std::initializer_list<DataLayoutElement> layout, const void* data, u32 dataSize, u32 alignment );

private:
	_private::OutputStreamImpl impl_;
};


class Node;
class NodeIterator;
class AttributeIterator;


// helper to check if binary blob is histream
class InputStreamHeader
{
public:
	//InputStreamHeader();
	InputStreamHeader( const u8* buf, size_t bufSize );

	const char* getMagic() const;

private:
	const _private::StreamHeader impl_;
};


// can be placed on stack
// doesn't allocate any heap memory
class InputStream
{
public:
	InputStream( const u8* buf, size_t bufSize );

	Node getRoot() const;

private:
	const _private::InputStreamImpl impl_;
};



// class for reading node data
class Node
{
public:
	Node();

	TagType tag() const;

	u32 numChildren() const;
	ObjectRange<NodeIterator> children() const;
	NodeIterator childrenBegin() const;
	NodeIterator childrenEnd() const;

	u32 numAttributes() const;
	ObjectRange<AttributeIterator> attributes() const;
	AttributeIterator attributesBegin() const;
	AttributeIterator attributesEnd() const;

private:
	Node( const _private::NodeHeader* node, const _private::InputStreamImpl* is );

private:
	const _private::NodeHeader* node_;
	const _private::InputStreamImpl* is_;

	friend class InputStream;
	friend class NodeIterator;
	friend class AttributeIterator;
};



// class for reading attributes
class Attribute
{
public:

	AttributeType::Type type() const;
	TagType tag() const;

	u8  getU8 () const;
	s8  getS8 () const;
	u16 getU16() const;
	s16 getS16() const;
	u32 getU32() const;
	s32 getS32() const;
	u64 getU64() const;
	s64 getS64() const;
	float getFloat() const;
	double getDouble() const;
	const char* getString( size_t& strLen ) const;

	// returns garbage for 'Data' and 'DataWithLayout' attributes
	u32 arrayLength() const;

	const u8*  u8Array () const;
	const s8*  s8Array () const;
	const u16* u16Array() const;
	const s16* s16Array() const;
	const u32* u32Array() const;
	const s32* s32Array() const;
	const u64* u64Array() const;
	const s64* s64Array() const;
	const float* floatArray() const;
	const double* doubleArray() const;

	const char* stringU8 ( size_t& strLen, u8&  val ) const;
	const char* stringS8 ( size_t& strLen, s8&  val ) const;
	const char* stringU16( size_t& strLen, u16& val ) const;
	const char* stringS16( size_t& strLen, s16& val ) const;
	const char* stringU32( size_t& strLen, u32& val ) const;
	const char* stringS32( size_t& strLen, s32& val ) const;
	const char* stringU64( size_t& strLen, u64& val ) const;
	const char* stringS64( size_t& strLen, s64& val ) const;
	const char* stringFloat ( size_t& strLen, float& val  ) const;
	const char* stringDouble( size_t& strLen, double& val ) const;

	// data* functions work only for 'Data' or 'DataWithLayout' attributes

	// data size in bytes
	u32 dataSize() const;
	u32 dataAlignment() const;
	const DataLayoutElement* dataLayout() const;
	size_t dataLayoutCount() const;
	// aligned on dataAlignment boundary
	const void* data() const;

private:
	Attribute();
	Attribute( const _private::AttributeHeader* attr, const _private::InputStreamImpl* is );

private:
	const _private::AttributeHeader* attr_;
	const _private::InputStreamImpl* is_;

	friend class Node;
	friend class AttributeIterator;
};




class NodeIterator
{
public:
	bool operator==( const NodeIterator& rhs ) const;
	bool operator!=( const NodeIterator& rhs ) const;
	const Node& operator*() const;
	const NodeIterator& operator++();

private:
	NodeIterator( const Node& node, u32 childIndex );

	Node node_;
	u32 childIndex_ = 0;

	friend class Node;
};




class AttributeIterator
{
public:
	bool operator==( const AttributeIterator& rhs ) const;
	bool operator!=( const AttributeIterator& rhs ) const;
	const Attribute& operator*() const;
	const Attribute* operator->() const;
	const AttributeIterator& operator++();

private:
	AttributeIterator( const Attribute& attr, u32 attrIndex );

	Attribute attr_;
	u32 attrIndex_ = 0;

	friend class Node;
};

} // namespace HiStream

#include "HiStream.inl"