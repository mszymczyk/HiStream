#pragma once

namespace _private
{

inline size_t alignPowerOfTwo( size_t val, size_t alignment )
{
	alignment--;
	return ( ( val + alignment ) & ~alignment );
}

inline u8* alignPowerOfTwo( u8* ptr, size_t alignment )
{
	alignment--;
	return reinterpret_cast<u8*>( ( (size_t)ptr + alignment ) & ~alignment );
}

inline const u8* alignPowerOfTwo( const u8* ptr, size_t alignment )
{
	alignment--;
	return reinterpret_cast<const u8*>( ( (size_t)ptr + alignment ) & ~alignment );
}

template<typename T>
bool validateAlign( const void* ptr )
{
	return ( (size_t)ptr & ( alignof( T ) - 1 ) ) == 0;
}

inline bool validateAlign( const void* ptr, size_t alignment )
{
	return ( (size_t)ptr & ( alignment - 1 ) ) == 0;
}


// https://stackoverflow.com/questions/600293/how-to-check-if-a-number-is-a-power-of-2
inline bool isPowerOfTwo( size_t x )
{
	return ( x != 0 ) && ( ( x & ( x - 1 ) ) == 0 );
}

void* default_memmory_alloc_func( size_t size, size_t alignment, void* userPtr );
void default_memmory_free_func( void* ptr, void* userPtr );


typedef u32 NodeOffsetType;
typedef u8 AttributeOffsetType;
typedef u32 AttributeOffsetLongType;


// attribute header - 8 bytes
struct __declspec(align(4)) AttributeHeader
{
	u8 tagIndex_;
	AttributeType::Type attrType_;
	u8 arraySize_;
	u8 offsetToNextAttribute_;
};


// use this header when AttributeHeader::arraySize_ == 255
struct AttributeHeaderLong
{
	u8 tagIndex_;
	AttributeType::Type attrType_;
	u8 arraySize_; // or num layout elements in case of Data or DataWithLayout
	u8 offsetToNextAttribute_; // or alignment in case of Data or DataWithLayout
	u32 arraySizeLong_;
	AttributeOffsetLongType offsetToNextAttributeLong_;
};


// node header - 20 bytes
struct NodeHeader
{
	TagType tag_ = 0;
	NodeOffsetType offsetToNextSibling_ = 0;
	NodeOffsetType offsetToFirstChild_ = 0;
	u32 nChildren_ = 0;
	u32 nAttributes_ = 0;
};


struct StreamHeader
{
	u8 magic_[8] = {};
	u32 offsetToTagRemapTable_ = 0;
	u32 nEntriesInTagRemapTable_ = 0;
};

struct OutputStreamImpl
{
	static const size_t maxAttrSize = 0xffffffff - sizeof( AttributeHeaderLong );
	static const size_t eNumTagIndices = 256;
	static const size_t invalidTagIndex = 0xffffffffffffffff;


	Allocator alloc_;
	Logger log_;
	size_t allocPageSize_ = 16 * 1024;

	u8* buf_ = nullptr;
	size_t bufUsedSize_ = 0;
	size_t bufCapacity_ = 0;
	//size_t paddingWastedSize_ = 0;
	size_t rootImpl_ = 0;

	enum { eStackDepth = 24 };
	size_t nodeStack_[eStackDepth] = {};
	size_t prevSiblingStack_[eStackDepth] = {};
	size_t stackCount_ = 0;

	size_t curNode_ = 0;
	size_t curAttribute_ = 0;

	Error::Type error_ = Error::noError;
	char errorText_[256] = {};

	TagType attrTag_[eNumTagIndices] = {};
	TagType attrTagSorted_[eNumTagIndices] = {};
	u8 attrTagSortedIndex_[eNumTagIndices] = {};
	u32 nAttrTag_ = 0;

	void error( Error::Type error, TagType attrTag, const char* format, ... );

	size_t findOrInsertAttrTagIndex( TagType tag );
	TagType attrTagIndexToTag( size_t index ) const { return attrTag_[index]; }

	u8* allocateMemImpl( size_t nBytes, size_t alignment, TagType tag );
	u8* allocateMem( size_t nBytes, size_t alignment, TagType tag )	{ return allocateMemImpl( nBytes, alignment, tag );	}
	template<typename T>
	T* allocateMem( TagType tag, size_t num = 1 ) {	return reinterpret_cast<T*>( allocateMemImpl( sizeof( T ) * num, alignof(T), tag ) ); }

	NodeHeader* allocateNode()	{ return reinterpret_cast<NodeHeader*>( allocateMemImpl( sizeof( NodeHeader ), alignof( NodeHeader ), 0 ) ); }
	NodeHeader* addNode( TagType tag );

	AttributeHeader* allocateAttr( TagType tag ) {	return reinterpret_cast<AttributeHeader*>( allocateMemImpl( sizeof( AttributeHeader ), alignof( AttributeHeader ), tag ) );	}
	AttributeHeaderLong* allocateAttrLong( TagType tag ) {	return reinterpret_cast<AttributeHeaderLong*>( allocateMemImpl( sizeof( AttributeHeaderLong ), alignof( AttributeHeaderLong ), tag ) ); }
	AttributeHeader* addAttr( size_t tagIndex, AttributeType::Type typ, u8 arraySize );
	AttributeHeaderLong* addAttrLong( size_t tagIndex, AttributeType::Type typ, u32 arraySize );

	AttributeOffsetType getAttrOffset( AttributeHeader* a, AttributeHeader* prevA ) const
	{
		size_t o = ((size_t)a - (size_t)prevA);
		HISTREAM_ASSERT( o < std::numeric_limits<AttributeOffsetType>::max() );
		return (AttributeOffsetType)( o );
	}

	AttributeOffsetType getAttrOffset( AttributeHeaderLong* a, AttributeHeader* prevA ) const
	{
		size_t o = ( (size_t)a - (size_t)prevA );
		HISTREAM_ASSERT( o < std::numeric_limits<AttributeOffsetType>::max() );
		return (AttributeOffsetType)( o );
	}

	AttributeOffsetLongType getAttrOffsetLong( AttributeHeader* a, AttributeHeader* prevA ) const
	{
		size_t o = (size_t)a - (size_t)prevA;
		HISTREAM_ASSERT( o < std::numeric_limits<AttributeOffsetLongType>::max() );
		return (AttributeOffsetLongType)( o );
	}

	AttributeOffsetLongType getAttrOffsetLong( AttributeHeaderLong* a, AttributeHeader* prevA ) const
	{
		size_t o = (size_t)a - (size_t)prevA;
		HISTREAM_ASSERT( o < std::numeric_limits<AttributeOffsetLongType>::max() );
		return (AttributeOffsetLongType)( o );
	}

	size_t getOffsetRelativeToStreamStart( const void* a ) const {	return (size_t)a - (size_t)buf_; }
	NodeHeader* getNode( size_t nodeOffset ) {	return reinterpret_cast<NodeHeader*>( buf_ + nodeOffset ); }
	NodeHeader* getCurNode() {	return reinterpret_cast<NodeHeader*>( buf_ + curNode_ ); }
	AttributeHeader* getAttribute( size_t attrOffset ) { return reinterpret_cast<AttributeHeader*>( buf_ + attrOffset ); }

	void pushStack( size_t nOffset );
	void popStack();

	void begin( const char magic[8] );
	void end();
	void pushChild( TagType tag );
	void popChild();
};


struct InputStreamImpl
{
	InputStreamImpl( const u8* buf, size_t bufSize )
		: buf_( buf )
		, bufSize_( bufSize )
		, header_( bufSize >= sizeof( StreamHeader )
								? reinterpret_cast<const StreamHeader*>( buf )
								: nullptr )

		, rootNode_( bufSize >= sizeof( StreamHeader )
								? reinterpret_cast<const _private::NodeHeader*>( buf + sizeof( StreamHeader ) )
								: nullptr )

		, attrTagIndexToTag_( bufSize >= sizeof( StreamHeader )
								? reinterpret_cast<const TagType*>( buf + header_->offsetToTagRemapTable_ )
								: nullptr )
	{	}

	const u8* buf_ = nullptr;
	const size_t bufSize_ = 0;
	const StreamHeader* header_ = nullptr;
	const NodeHeader* rootNode_ = nullptr;
	const TagType* attrTagIndexToTag_ = nullptr;
	const u32 nAttrTagIndexToTag_ = 0;

	TagType tagIndexToType( size_t tagIndex ) const { return attrTagIndexToTag_[tagIndex]; }
};

} // namespace _private






  // Range-based for loop support
template <typename It>
class ObjectRange
{
public:
	typedef It const_iterator;
	typedef It iterator;

	ObjectRange( It b, It e ) : begin_( b ), end_( e )
	{
	}

	It begin() const {
		return begin_;
	}
	It end() const {
		return end_;
	}

private:
	It begin_, end_;
};
