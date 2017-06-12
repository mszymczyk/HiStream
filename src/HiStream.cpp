#include "HiStream.h"
#include <array>

namespace HiStream
{

template<typename T, uint32_t count, T invalidId>
class StringToIdMap
{
public:
	StringToIdMap( const char* idToString[], size_t n )
	{
		for ( size_t i = 0; i < n; ++i )
		{
			Entry& e = entries_[i];
			e.name_ = idToString[i];
			e.nameLen_ = static_cast<u32>( strlen( e.name_ ) );
			e.id_ = static_cast<T>( i );
		}

		std::sort( std::begin( entries_ ), std::end( entries_ ), Entry_compare() );
	}

	T GetId( const char* name ) const
	{
		Entry e;
		e.name_ = name;
		e.nameLen_ = static_cast<u32>( strlen( name ) );

		Entry_compare predicate;
		typename EntriesArray::const_iterator it = std::lower_bound( std::cbegin( entries_ ), std::cend( entries_ ), e, predicate );
		// assert when name is not found
		if ( it != entries_.end() && !predicate( e, *it ) )
			return (T)it->id_;
		else
			return (T)invalidId;
	}

private:
	struct Entry
	{
		const char* name_ = nullptr;
		u32 nameLen_ = 0;
		T id_;

		const char* getName() const { return name_; }
	};

	struct Entry_compare
	{
		bool operator() ( const Entry& lhs, const Entry& rhs ) const
		{
			size_t lLen = lhs.nameLen_;
			size_t rLen = rhs.nameLen_;
			if ( lLen < rLen )
				return true;
			else if ( lLen == rLen )
				return memcmp( lhs.getName(), rhs.getName(), lLen ) < 0;

			return false;

			// this gives alphabetical order but is slower
			//return strcmp( lhs.getName(), rhs.getName() ) < 0;
		}
	};

	typedef std::array<Entry, count> EntriesArray;
	EntriesArray entries_;
};

namespace AttributeType
{
static const char* _attrIdToString[count] = {
	  "Invalid"

	// basic types
	, "U8"
	, "S8"
	, "U16"
	, "S16"
	, "U32"
	, "S32"
	, "U64"
	, "S64"
	, "Float"
	, "Double"
	, "String"

	// basic arrays
	, "U8Array"
	, "S8Array"
	, "U16Array"
	, "S16Array"
	, "U32Array"
	, "S32Array"
	, "U64Array"
	, "S64Array"
	, "FloatArray"
	, "DoubleArray"
	//, "StringArray"

	// string + basic types
	, "StringU8"
	, "StringS8"
	, "StringU16"
	, "StringS16"
	, "StringU32"
	, "StringS32"
	, "StringU64"
	, "StringS64"
	, "StringFloat"
	, "StringDouble"

	, "Data"
	, "DataWithLayout"

};


static StringToIdMap<Type, count, Invalid> _stringToId( _attrIdToString, count );

const char* ToString( Type type )
{
	return _attrIdToString[type];
}

Type FromString( const char* str )
{
	return _stringToId.GetId( str );
}

} // namespace AttributeType



namespace Error
{
static const char* _IdToString[count] = {
	  "noError"
	, "noMem"
	, "dataOverflow"
	, "attrChildOrder"
	, "hierarchyOverflow"
	, "hierarchyCorrupted"
	, "badLayout"
	, "tagOverflow"
	, "badAlign"
};

const char* ToString( Type type )
{
	return _IdToString[type];
}

} // namespace Error


#define errorAttributeOverflow "attribute size overflow (max %u bytes per attribute)", (u32)_private::OutputStreamImpl::maxAttrSize
#define errorDataAlignment "alignment must be power of two and less-equal 64"


namespace DataType
{
static const char* _elementTypeToString[count] = {
	"Invalid",
	"U8",
	"S8",
	"U16",
	"S16",
	"U32",
	"S32",
	"U64",
	"S64",
	"Float",
	"Double",
};

static u32 _elementTypeSize[count] = {
	0,
	1,
	1,
	2,
	2,
	4,
	4,
	8,
	8,
	4,
	8,
};

static StringToIdMap<Type, count, Invalid> _stringToId( _elementTypeToString, count );

const char* ToString( Type type )
{
	return _elementTypeToString[type];
}

Type FromString( const char* str )
{
	return _stringToId.GetId( str );
}

u32 SizeInBytes( Type type )
{
	return _elementTypeSize[type];
}

} // namespace DataElementType


template<typename T>
inline size_t _CountAllocReq( size_t curSiz, size_t num = 1 )
{
	size_t alignedCurSize = _private::alignPowerOfTwo( curSiz, alignof(T) );
	//paddingWastedSize_ += alignedCurSize - curSiz;
	return alignedCurSize + sizeof( T ) * num;
}

inline size_t _CountAllocReq( size_t curSiz, size_t size, size_t alignment )
{
	size_t alignedCurSize = _private::alignPowerOfTwo( curSiz, alignment );
	return alignedCurSize + size;
}

template<typename T>
u8* _SetNumberArray( u8* ptr, const T* t, size_t n )
{
	HISTREAM_ASSERT( _private::validateAlign<T>( ptr ) );
	if ( t )
		memcpy( ptr, t, sizeof( T ) * n );
	// allocated memory is always cleared to 0
	return ptr + sizeof( T ) * n;
}

//template<typename T
//	, typename std::enable_if< std::is_same<T, u8>::value, int >::type = 0>
//	AttributeType::Type _TypeToAttributeType()
//{
//	return AttributeType::U8;
//}

//template<typename T
//	, typename std::enable_if< std::is_same<T, float>::value, int >::type = 0>
//	AttributeType::Type _TypeToAttributeType()
//{
//	return AttributeType::Float;
//}

template<typename T>
void _AddType( _private::OutputStreamImpl& impl, TagType tag, AttributeType::Type atyp, T x )
{
	if ( impl.error_ )
		return;

	size_t tagIndex = impl.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return;

	impl.addAttr( tagIndex, atyp, 1 );

	if ( impl.error_ )
		return;

	T* mem = impl.allocateMem<T>( tag );

	if ( !mem )
		return;

	*mem = x;
}

//template<typename T>
//void _AddType( TagType tag, T x )
//{
//	if ( error_ )
//		return;

//	size_t tagIndex = _FindOrInsertAttrTagIndex( tag );
//	if ( tagIndex == invalidTagIndex )
//		return;

//	AttributeType::Type atyp = _TypeToAttributeType<T>();
//	addAttr( tagIndex, atyp, 1 );

//	if ( error_ )
//		return;

//	T* mem = allocateMem<T>();

//	if ( !mem )
//		return;

//	*mem = x;
//}

template<typename T>
T* _AddTypeArray( _private::OutputStreamImpl& impl, TagType tag, AttributeType::Type atyp, const T* arr, u32 num )
{
	if ( impl.error_ )
		return nullptr;

	// estimate size of this attribute, err if too big
	size_t estSize = sizeof( _private::AttributeHeaderLong );
	estSize = _CountAllocReq<T>( estSize, num );
	estSize = _private::alignPowerOfTwo( estSize, alignof( _private::AttributeHeader ) ); // next attribute will be aligned on AttributeHeader boundary
	if ( estSize > _private::OutputStreamImpl::maxAttrSize )
	{
		impl.error( Error::dataOverflow, tag, errorAttributeOverflow );
		return nullptr;
	}

	size_t tagIndex = impl.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return nullptr;

	if ( num * sizeof( T ) + sizeof( _private::AttributeHeader ) > 255 )
		impl.addAttrLong( tagIndex, atyp, num );
	else
		impl.addAttr( tagIndex, atyp, static_cast<u8>( num ) );

	if ( impl.error_ )
		return nullptr;

	u8* mem = (u8*)impl.allocateMem<T>( tag, num );

	if ( !mem )
		return nullptr;

	_SetNumberArray( mem, arr, num );
	return reinterpret_cast<T*>( mem );
}

template<typename T>
void _AddStringType( _private::OutputStreamImpl& impl, TagType tag, AttributeType::Type atyp, const char* str, size_t strLen, T x )
{
	if ( impl.error_ )
		return;

	// estimate size of this attribute, err if too big
	size_t estSize = sizeof( _private::AttributeHeaderLong );
	estSize += strLen + 1;
	estSize = _CountAllocReq<T>( estSize );
	estSize = _private::alignPowerOfTwo( estSize, alignof( _private::AttributeHeader ) ); // next attribute will be aligned on AttributeHeader boundary
	if ( estSize > _private::OutputStreamImpl::maxAttrSize )
	{
		impl.error( Error::dataOverflow, tag, errorAttributeOverflow );
		return;
	}

	size_t tagIndex = impl.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return;

	if ( strLen + 1 + sizeof( T ) + sizeof( _private::AttributeHeader ) > 255 )
		impl.addAttrLong( tagIndex, atyp, static_cast<u32>( strLen ) );
	else
		impl.addAttr( tagIndex, atyp, static_cast<u8>( strLen ) );

	if ( impl.error_ )
		return;

	size_t req = strLen + 1;
	req = _CountAllocReq<T>( req );

	u8* mem = impl.allocateMem( req, alignof( T ), tag );

	if ( !mem )
		return;

	memcpy( mem, str, strLen );
	mem[strLen] = '\0';
	mem = _private::alignPowerOfTwo( mem + strLen + 1, alignof( T ) );
	*reinterpret_cast<T*>( mem ) = x;
}

inline const u8* _GetArrayAddress( const _private::AttributeHeader* attr, size_t alignment )
{
	const u8* mem;
	if ( attr->arraySize_ == 255 )
		mem = reinterpret_cast<const u8*>( reinterpret_cast<const _private::AttributeHeaderLong*>( attr ) + 1 );
	else
		mem = reinterpret_cast<const u8*>( attr + 1 );

	return _private::alignPowerOfTwo( mem, alignment );
}

//inline u32 _GetArraySize( const _private::AttributeHeader* attr )
//{
//	return attr->arraySize_ != 255 ? attr->arraySize_ : reinterpret_cast<const _private::AttributeHeaderLong*>( attr )->arraySizeLong_;
//}

//inline const u8* _GetTypeArray( const _private::AttributeHeader* attr, AttributeType::Type type, size_t alignment )
//{
//	if ( attr->attrType_ != type )
//		return nullptr;
//
//	return _GetArrayAddress( attr, alignment );
//}

template<typename T>
inline const T* _GetTypeArray( const _private::AttributeHeader* attr, AttributeType::Type type )
{
	if ( attr->attrType_ != type )
		return nullptr;

	return reinterpret_cast<const T*>( _GetArrayAddress( attr, alignof( T ) ) );
}

template<typename T>
inline T _GetType( const _private::AttributeHeader* attr, AttributeType::Type type )
{
	if ( attr->attrType_ != type )
		return T{};

	return *reinterpret_cast<const T*>( _private::alignPowerOfTwo( reinterpret_cast<const u8*>( attr + 1 ), alignof( T ) ) );
}

template<typename T>
inline const char* _GetStringType( const _private::AttributeHeader* attr, AttributeType::Type type, size_t& strLen, T& val )
{
	if ( attr->attrType_ != type )
		return nullptr;

	const u8* str;
	if ( attr->arraySize_ != 255 )
	{
		strLen = attr->arraySize_;
		str = reinterpret_cast<const u8*>( attr + 1 );
	}
	else
	{
		strLen = reinterpret_cast<const _private::AttributeHeaderLong*>( attr )->arraySizeLong_;
		str = reinterpret_cast<const u8*>( reinterpret_cast<const _private::AttributeHeaderLong*>( attr ) + 1 );
	}

	str = _private::alignPowerOfTwo( str, alignof( T ) );

	val = *reinterpret_cast<const T*>( _private::alignPowerOfTwo( str + strLen + 1, alignof( T ) ) );
	return reinterpret_cast<const char*>( str );
}


OutputStream::OutputStream( const Allocator* alloc /*= nullptr*/, const Logger* log /*= nullptr*/ )
{
	if ( alloc )
	{
		impl_.alloc_ = *alloc;
	}
	else
	{
		impl_.alloc_.alloc_ = _private::default_memmory_alloc_func;
		impl_.alloc_.free_ = _private::default_memmory_free_func;
	}

	if ( log )
		impl_.log_ = *log;
}

OutputStream::~OutputStream()
{
	impl_.alloc_.free_( impl_.buf_, impl_.alloc_.userPtr_ );
}

void OutputStream::begin()
{
	impl_.begin( "histr10" );
}

void OutputStream::end()
{
	impl_.end();
}

const u8* OutputStream::buffer() const
{
	return impl_.buf_;
}

size_t OutputStream::bufferSize() const
{
	return impl_.bufUsedSize_;
}

u8* OutputStream::stealBuffer()
{
	HISTREAM_ASSERT( impl_.stackCount_ == 0 );
	u8* ret = impl_.buf_;
	impl_.buf_ = nullptr;
	impl_.bufUsedSize_ = 0;
	impl_.bufCapacity_ = 0;
	//impl_.paddingWastedSize_ = 0;
	return ret;
}


HiStream::Allocator OutputStream::alloc() const
{
	return impl_.alloc_;
}

void OutputStream::pushChild( TagType tag )
{
	impl_.pushChild( tag );
}

void OutputStream::popChild()
{
	impl_.popChild();
}

void OutputStream::addU8( TagType tag, u8 x )
{
	_AddType( impl_, tag, AttributeType::U8, x );
}

void OutputStream::addS8( TagType tag, s8 x )
{
	_AddType( impl_, tag, AttributeType::S8, x );
}

void OutputStream::addU16( TagType tag, u16 x )
{
	_AddType( impl_, tag, AttributeType::U16, x );
}

void OutputStream::addS16( TagType tag, s16 x )
{
	_AddType( impl_, tag, AttributeType::S16, x );
}

void OutputStream::addU32( TagType tag, u32 x )
{
	_AddType( impl_, tag, AttributeType::U32, x );
}

void OutputStream::addS32( TagType tag, s32 x )
{
	_AddType( impl_, tag, AttributeType::S32, x );
}

void OutputStream::addU64( TagType tag, u64 x )
{
	_AddType( impl_, tag, AttributeType::U64, x );
}

void OutputStream::addS64( TagType tag, s64 x )
{
	_AddType( impl_, tag, AttributeType::S64, x );
}

void OutputStream::addFloat( TagType tag, float x )
{
	_AddType( impl_, tag, AttributeType::Float, x );
}

void OutputStream::addDouble( TagType tag, double x )
{
	_AddType( impl_, tag, AttributeType::Double, x );
}

void OutputStream::addString( TagType tag, const char* str, size_t strLen )
{
	if ( impl_.error_ )
		return;

	// estimate size of this attribute, err if too big
	size_t estSize = sizeof( _private::AttributeHeaderLong );
	estSize += strLen + 1;
	estSize = _private::alignPowerOfTwo( estSize, alignof( _private::AttributeHeader ) ); // next attribute will be aligned on AttributeHeader boundary
	if ( estSize > _private::OutputStreamImpl::maxAttrSize )
	{
		impl_.error( Error::dataOverflow, tag, errorAttributeOverflow );
		return;
	}

	size_t tagIndex = impl_.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return;

	if ( sizeof( _private::AttributeHeader ) + strLen + 1 > 255 )
		impl_.addAttrLong( tagIndex, AttributeType::String, static_cast<u32>( strLen ) );
	else
		impl_.addAttr( tagIndex, AttributeType::String, static_cast<u8>( strLen ) );

	if ( impl_.error_ )
		return;

	u8* mem = (u8*)impl_.allocateMem<char>( tag, strLen + 1 );

	if ( !mem )
		return;

	memcpy( mem, str, strLen );
	mem[strLen] = '\0';
}

u8* OutputStream::addU8Array( TagType tag, const u8* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::U8Array, arr, num );
}

s8* OutputStream::addS8Array( TagType tag, const s8* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::S8Array, arr, num );
}

u16* OutputStream::addU16Array( TagType tag, const u16* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::U16Array, arr, num );
}

s16* OutputStream::addS16Array( TagType tag, const s16* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::S16Array, arr, num );
}

u32* OutputStream::addU32Array( TagType tag, const u32* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::U32Array, arr, num );
}

s32* OutputStream::addS32Array( TagType tag, const s32* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::S32Array, arr, num );
}

u64* OutputStream::addU64Array( TagType tag, const u64* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::U64Array, arr, num );
}

s64* OutputStream::addS64Array( TagType tag, const s64* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::S64Array, arr, num );
}

float* OutputStream::addFloatArray( TagType tag, const float* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::FloatArray, arr, num );
}

double* OutputStream::addDoubleArray( TagType tag, const double* arr, u32 num )
{
	return _AddTypeArray( impl_, tag, AttributeType::DoubleArray, arr, num );
}

void OutputStream::addStringU8( TagType tag, const char* str, size_t strLen, u8 x )
{
	_AddStringType( impl_, tag, AttributeType::StringU8, str, strLen, x );
}

void OutputStream::addStringS8( TagType tag, const char* str, size_t strLen, s8 x )
{
	_AddStringType( impl_, tag, AttributeType::StringS8, str, strLen, x );
}

void OutputStream::addStringU16( TagType tag, const char* str, size_t strLen, u16 x )
{
	_AddStringType( impl_, tag, AttributeType::StringU16, str, strLen, x );
}

void OutputStream::addStringS16( TagType tag, const char* str, size_t strLen, s16 x )
{
	_AddStringType( impl_, tag, AttributeType::StringS16, str, strLen, x );
}

void OutputStream::addStringU32( TagType tag, const char* str, size_t strLen, u32 x )
{
	_AddStringType( impl_, tag, AttributeType::StringU32, str, strLen, x );
}

void OutputStream::addStringS32( TagType tag, const char* str, size_t strLen, s32 x )
{
	_AddStringType( impl_, tag, AttributeType::StringS32, str, strLen, x );
}

void OutputStream::addStringU64( TagType tag, const char* str, size_t strLen, u64 x )
{
	_AddStringType( impl_, tag, AttributeType::StringU64, str, strLen, x );
}

void OutputStream::addStringS64( TagType tag, const char* str, size_t strLen, s64 x )
{
	_AddStringType( impl_, tag, AttributeType::StringS64, str, strLen, x );
}

void OutputStream::addStringFloat( TagType tag, const char* str, size_t strLen, float x )
{
	_AddStringType( impl_, tag, AttributeType::StringFloat, str, strLen, x );
}

void OutputStream::addStringDouble( TagType tag, const char* str, size_t strLen, double x )
{
	_AddStringType( impl_, tag, AttributeType::StringDouble, str, strLen, x );
}

void* OutputStream::addData( TagType tag, const void* data, u32 dataSize, u32 alignment )
{
	if ( impl_.error_ )
		return nullptr;

	if ( !_private::isPowerOfTwo( alignment ) || alignment > 64 )
	{
		impl_.error( Error::badAlign, tag, errorDataAlignment );
		return nullptr;
	}

	// estimate size of this attribute, err if too big
	size_t estSize = sizeof( _private::AttributeHeaderLong );
	estSize = _private::alignPowerOfTwo( estSize, alignment );
	estSize += dataSize;
	estSize = _private::alignPowerOfTwo( estSize, alignof( _private::AttributeHeader ) ); // next attribute will be aligned on AttributeHeader boundary
	if ( estSize > _private::OutputStreamImpl::maxAttrSize )
	{
		impl_.error( Error::dataOverflow, tag, errorAttributeOverflow );
		return nullptr;
	}

	size_t tagIndex = impl_.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return nullptr;

	_private::AttributeHeaderLong* ahl = impl_.addAttrLong( tagIndex, AttributeType::Data, dataSize );

	if ( impl_.error_ )
		return nullptr;

	ahl->offsetToNextAttribute_ = static_cast<u8>( alignment );

	u8* mem = impl_.allocateMem( dataSize, alignment, tag );
	if ( !mem )
		return nullptr;

	HISTREAM_ASSERT( mem + dataSize <= impl_.buf_ + impl_.bufUsedSize_ );
	HISTREAM_ASSERT( _private::validateAlign( mem, alignment ) );
	if ( data )
		memcpy( mem, data, dataSize );

	return mem;
}

void* OutputStream::addDataWithLayout( TagType tag, const DataLayoutElement* layout, size_t nLayout, const void* data, u32 dataSize, u32 alignment )
{
	if ( impl_.error_ )
		return nullptr;

	if ( !_private::isPowerOfTwo( alignment ) || alignment > 64 )
	{
		impl_.error( Error::badAlign, tag, errorDataAlignment );
		return nullptr;
	}

	size_t neededAlign = 0;
	size_t elementSize = 0;
	for ( size_t i = 0; i < nLayout; ++i )
	{
		size_t s = DataType::SizeInBytes( layout[i].type );
		elementSize += s * layout[i].nWords;
		if ( s > neededAlign )
			neededAlign = s;
	}

	if ( nLayout == 0 || nLayout >= 0xff || elementSize == 0 )
	{
		impl_.error( Error::badLayout, tag, "too many layout items" );
		return nullptr;
	}

	if ( alignment < neededAlign )
	{
		impl_.error( Error::badAlign, tag, "layout requires at least %u-byte align", neededAlign );
		return nullptr;
	}

	// estimate size of this attribute, err if too big
	size_t estSize = sizeof( _private::AttributeHeaderLong );
	estSize = _CountAllocReq<DataLayoutElement>( estSize, nLayout ); // next attribute will be aligned on AttributeHeader boundary
	estSize = _private::alignPowerOfTwo( estSize, alignment );
	estSize += dataSize;
	estSize = _private::alignPowerOfTwo( estSize, alignof( _private::AttributeHeader ) ); // next attribute will be aligned on AttributeHeader boundary
	if ( estSize > _private::OutputStreamImpl::maxAttrSize )
	{
		impl_.error( Error::dataOverflow, tag, errorAttributeOverflow );
		return nullptr;
	}

	size_t n = dataSize / elementSize;
	if ( n * elementSize != dataSize )
	{
		// layout doesn't match data size/alignment
		impl_.error( Error::badLayout, tag, "layout doesn't match data (hidden padding?)" );
		return nullptr;
	}

	size_t tagIndex = impl_.findOrInsertAttrTagIndex( tag );
	if ( tagIndex == _private::OutputStreamImpl::invalidTagIndex )
		return nullptr;

	_private::AttributeHeaderLong* a = impl_.addAttrLong( tagIndex, AttributeType::DataWithLayout, dataSize );

	if ( impl_.error_ )
		return nullptr;

	a->arraySize_ = static_cast<u8>( nLayout );
	a->offsetToNextAttribute_ = static_cast<u8>( alignment );

	size_t layoutSize = nLayout * sizeof( DataLayoutElement );
	u8* mem = impl_.allocateMem( layoutSize, alignof( DataLayoutElement ), tag );
	if ( !mem )
		return nullptr;

	memcpy( mem, layout, layoutSize );

	mem = impl_.allocateMem( dataSize, alignment, tag );
	if ( !mem )
		return nullptr;

	HISTREAM_ASSERT( mem + dataSize <= impl_.buf_ + impl_.bufUsedSize_ );
	HISTREAM_ASSERT( _private::validateAlign( mem, alignment ) );
	if ( data )
		memcpy( mem, data, dataSize );

	return mem;
}

u8 Attribute::getU8() const
{
	return _GetType<u8>( attr_, AttributeType::U8 );
}

s8 Attribute::getS8() const
{
	return _GetType<s8>( attr_, AttributeType::S8 );
}

u16 Attribute::getU16() const
{
	return _GetType<u16>( attr_, AttributeType::U16 );
}

s16 Attribute::getS16() const
{
	return _GetType<s16>( attr_, AttributeType::S16 );
}

u32 Attribute::getU32() const
{
	return _GetType<u32>( attr_, AttributeType::U32 );
}

s32 Attribute::getS32() const
{
	return _GetType<s32>( attr_, AttributeType::S32 );
}

u64 Attribute::getU64() const
{
	return _GetType<u64>( attr_, AttributeType::U64 );
}

s64 Attribute::getS64() const
{
	return _GetType<s64>( attr_, AttributeType::S64 );
}

float Attribute::getFloat() const
{
	return _GetType<float>( attr_, AttributeType::Float );
}

double Attribute::getDouble() const
{
	return _GetType<double>( attr_, AttributeType::Double );
}

const u8* Attribute::u8Array() const
{
	return _GetTypeArray<u8>( attr_, AttributeType::U8Array );
}

const s8* Attribute::s8Array() const
{
	return _GetTypeArray<s8>( attr_, AttributeType::S8Array );
}

const u16* Attribute::u16Array() const
{
	return _GetTypeArray<u16>( attr_, AttributeType::U16Array );
}

const s16* Attribute::s16Array() const
{
	return _GetTypeArray<s16>( attr_, AttributeType::S16Array );
}

const u32* Attribute::u32Array() const
{
	return _GetTypeArray<u32>( attr_, AttributeType::U32Array );
}

const s32* Attribute::s32Array() const
{
	return _GetTypeArray<s32>( attr_, AttributeType::S32Array );
}

const u64* Attribute::u64Array() const
{
	return _GetTypeArray<u64>( attr_, AttributeType::U64Array );
}

const s64* Attribute::s64Array() const
{
	return _GetTypeArray<s64>( attr_, AttributeType::S64Array );
}

const float* Attribute::floatArray() const
{
	return _GetTypeArray<float>( attr_, AttributeType::FloatArray );
}

const double* Attribute::doubleArray() const
{
	return _GetTypeArray<double>( attr_, AttributeType::DoubleArray );
}

const char* Attribute::getString( size_t& strLen ) const
{
	if ( attr_->attrType_ != AttributeType::String )
		return nullptr;

	if ( attr_->arraySize_ != 255 )
	{
		strLen = attr_->arraySize_;
		return reinterpret_cast<const char*>( attr_ + 1 );
	}
	else
	{
		strLen = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ )->arraySizeLong_;
		return reinterpret_cast<const char*>( reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ ) + 1 );
	}
}

const char* Attribute::stringU8( size_t& strLen, u8& val ) const
{
	return _GetStringType( attr_, AttributeType::StringU8, strLen, val );
}

const char* Attribute::stringS8( size_t& strLen, s8& val ) const
{
	return _GetStringType( attr_, AttributeType::StringS8, strLen, val );
}

const char* Attribute::stringU16( size_t& strLen, u16& val ) const
{
	return _GetStringType( attr_, AttributeType::StringU16, strLen, val );
}

const char* Attribute::stringS16( size_t& strLen, s16& val ) const
{
	return _GetStringType( attr_, AttributeType::StringS16, strLen, val );
}

const char* Attribute::stringU32( size_t& strLen, u32& val ) const
{
	return _GetStringType( attr_, AttributeType::StringU32, strLen, val );
}

const char* Attribute::stringS32( size_t& strLen, s32& val ) const
{
	return _GetStringType( attr_, AttributeType::StringS32, strLen, val );
}

const char* Attribute::stringU64( size_t& strLen, u64& val ) const
{
	return _GetStringType( attr_, AttributeType::StringU64, strLen, val );
}

const char* Attribute::stringS64( size_t& strLen, s64& val ) const
{
	return _GetStringType( attr_, AttributeType::StringS64, strLen, val );
}

const char* Attribute::stringFloat( size_t& strLen, float& val ) const
{
	return _GetStringType( attr_, AttributeType::StringFloat, strLen, val );
}

const char* Attribute::stringDouble( size_t& strLen, double& val ) const
{
	return _GetStringType( attr_, AttributeType::StringDouble, strLen, val );
}

u32 Attribute::dataSize() const
{
	if ( attr_->attrType_ != AttributeType::Data && attr_->attrType_ != AttributeType::DataWithLayout )
		return 0;

	HISTREAM_ASSERT( (attr_->attrType_ == AttributeType::Data && attr_->arraySize_ == 255) || ( attr_->attrType_ == AttributeType::DataWithLayout && attr_->arraySize_ < 255 ) );
	return reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ )->arraySizeLong_;
}

u32 Attribute::dataAlignment() const
{
	if ( attr_->attrType_ != AttributeType::Data && attr_->attrType_ != AttributeType::DataWithLayout )
		return 0;

	HISTREAM_ASSERT( ( attr_->attrType_ == AttributeType::Data && attr_->arraySize_ == 255 ) || ( attr_->attrType_ == AttributeType::DataWithLayout && attr_->arraySize_ < 255 ) );
	return reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ )->offsetToNextAttribute_;
}

const DataLayoutElement* Attribute::dataLayout() const
{
	if ( attr_->attrType_ != AttributeType::DataWithLayout )
		return nullptr;

	const _private::AttributeHeaderLong* a = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ );
	const u8* mem = reinterpret_cast<const u8*>( a + 1 );
	return reinterpret_cast<const DataLayoutElement*>( _private::alignPowerOfTwo( mem, alignof(DataLayoutElement) ) );
}

size_t Attribute::dataLayoutCount() const
{
	if ( attr_->attrType_ != AttributeType::DataWithLayout )
		return 0;

	const _private::AttributeHeaderLong* a = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ );
	return a->arraySize_;
}

const void* Attribute::data() const
{
	if ( attr_->attrType_ == AttributeType::Data )
	{
		HISTREAM_ASSERT( attr_->arraySize_ == 255 );
		const _private::AttributeHeaderLong* a = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ );
		const u8* mem = reinterpret_cast<const u8*>( a + 1 );
		mem = _private::alignPowerOfTwo( mem, a->offsetToNextAttribute_ );
		return mem;
	}
	else if ( attr_->attrType_ == AttributeType::DataWithLayout )
	{
		const _private::AttributeHeaderLong* a = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ );
		const u8* mem = reinterpret_cast<const u8*>( a + 1 );
		mem = _private::alignPowerOfTwo( mem, alignof( DataLayoutElement ) );
		mem += a->arraySize_ * sizeof( DataLayoutElement );
		mem = _private::alignPowerOfTwo( mem, a->offsetToNextAttribute_ );
		return mem;
	}
	else
		return nullptr;
}

} // namespace HiStream