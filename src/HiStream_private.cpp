#include "HiStream.h"
#include <stdarg.h>
#include <algorithm>

namespace HiStream
{

namespace _private
{

#define errorNodeOverflow "node size overflow (max %llu bytes per node)", std::numeric_limits<NodeOffsetType>::max()
#define errorPushPop "pushChild/popChild mismatch"


void* default_memmory_alloc_func( size_t size, size_t alignment, void* /*userPtr*/ )
{
	return _aligned_malloc( size, alignment );
}

void default_memmory_free_func( void* ptr, void* /*userPtr*/ )
{
	_aligned_free( ptr );
}

void OutputStreamImpl::error( Error::Type error, TagType attrTag, const char* format, ... )
{
	va_list	args;

	va_start( args, format );
	char buffer[sizeof( errorText_ )];
	/*int nWritten =*/ vsnprintf( buffer, sizeof( buffer ) - 1, format, args );
	buffer[sizeof( errorText_ ) - 1] = '\0';
	va_end( args );

	char buffer2[sizeof( buffer )];
	char nodeTagBuf[5];
	char attrTagBuf[5];
	NodeHeader* cn = getCurNode();
	snprintf( buffer2, sizeof(buffer2), "%s (node = %s, attr = %s)", buffer, cn ? TagToStr( cn->tag_, nodeTagBuf ) : "", attrTag ? TagToStr( attrTag, attrTagBuf ) : "" );

	if ( log_.logError_ )
		log_.logError_( buffer2 );

	if ( error_ == Error::noError )
	{
		error_ = error;
		memcpy( errorText_, buffer2, sizeof( errorText_ ) );
	}
}

size_t OutputStreamImpl::findOrInsertAttrTagIndex( TagType tag )
{
	TagType* found = std::lower_bound( attrTagSorted_, attrTagSorted_ + nAttrTag_, tag );
	if ( ( found != ( attrTagSorted_ + nAttrTag_ ) ) && *found == tag )
		return attrTagSortedIndex_[( (size_t)found - (size_t)attrTagSorted_ ) / sizeof( TagType )];

	if ( nAttrTag_ == eNumTagIndices )
	{
		error( Error::tagOverflow, tag, "too many unique tags" );
		return invalidTagIndex;
	}

	size_t foundIndex = ((size_t)found - (size_t)attrTagSorted_ ) / sizeof(TagType);

	for ( size_t i = nAttrTag_; i != foundIndex; --i )
	{
		attrTagSorted_[i] = attrTagSorted_[i - 1];
		attrTagSortedIndex_[i] = attrTagSortedIndex_[i - 1];
	}

	attrTagSorted_[foundIndex] = tag;
	attrTagSortedIndex_[foundIndex] = static_cast<u8>( nAttrTag_ );
	attrTag_[nAttrTag_] = tag;
	return nAttrTag_++;
}

u8* OutputStreamImpl::allocateMemImpl( size_t nBytes, size_t alignment, TagType tag )
{
	if ( error_ )
		return nullptr;

	size_t bufSizeAligned = alignPowerOfTwo( bufUsedSize_, alignment );

	//paddingWastedSize_ += bufSizeAligned - bufUsedSize_;
	size_t newBufSize = bufSizeAligned + nBytes;;
	if ( newBufSize <= bufCapacity_ )
	{
		u8* p = buf_ + bufSizeAligned;
		bufUsedSize_ = newBufSize;
		return p;
	}

	size_t newBufCapacity = alignPowerOfTwo( newBufSize, allocPageSize_ );
	u8* newBuf = reinterpret_cast<u8*>( alloc_.alloc_( newBufCapacity, 64, alloc_.userPtr_ ) );
	if ( !newBuf )
	{
		error( Error::noMem, tag, "couldn't allocate memory (%llu bytes).", newBufCapacity );
		return nullptr;
	}

	bufCapacity_ = newBufCapacity;

	if ( buf_ )
		memcpy( newBuf, buf_, bufUsedSize_ );

	memset( newBuf + bufUsedSize_, 0, bufCapacity_ - bufUsedSize_ );

	alloc_.free_( buf_, alloc_.userPtr_ );
	buf_ = newBuf;

	u8* p = buf_ + bufSizeAligned;
	bufUsedSize_ = newBufSize;
	return p;
}

NodeHeader* OutputStreamImpl::addNode( TagType tag )
{
	NodeHeader* n = allocateNode();
	if ( !n )
		return nullptr;

	n->tag_ = tag;
	n->offsetToNextSibling_ = 0;
	n->offsetToFirstChild_ = 0;
	n->nChildren_ = 0;
	n->nAttributes_ = 0;
	return n;
}

AttributeHeader* OutputStreamImpl::addAttr( size_t tagIndex, AttributeType::Type typ, u8 arraySize )
{
	// attributes must be added before any children
	if ( getCurNode()->nChildren_ != 0 )
	{
		error( Error::attrChildOrder, attrTagIndexToTag( tagIndex ), "attributes must be added before child nodes" );
		return nullptr;
	}

	AttributeHeader* a = allocateAttr( attrTagIndexToTag( tagIndex ) );
	if ( !a )
		return nullptr;

	HISTREAM_ASSERT( tagIndex <= 255 );
	a->tagIndex_ = static_cast<u8>( tagIndex );
	a->attrType_ = typ;
	a->arraySize_ = arraySize;
	a->offsetToNextAttribute_ = 0;
	if ( curAttribute_ )
	{
		AttributeHeader* ca = getAttribute( curAttribute_ );
		if ( ca->arraySize_ == 255 || ca->attrType_ == AttributeType::DataWithLayout )
			reinterpret_cast<AttributeHeaderLong*>( ca )->offsetToNextAttributeLong_ = getAttrOffsetLong( a, ca );
		else
			ca->offsetToNextAttribute_ = getAttrOffset( a, ca );
	}
	curAttribute_ = getOffsetRelativeToStreamStart( a );
	++getCurNode()->nAttributes_;
	return a;
}

AttributeHeaderLong* OutputStreamImpl::addAttrLong( size_t tagIndex, AttributeType::Type typ, u32 arraySize )
{
	// attributes must be added before any children
	if ( getCurNode()->nChildren_ != 0 )
	{
		error( Error::attrChildOrder, attrTagIndexToTag( tagIndex ), "attributes must be added before child nodes" );
		return nullptr;
	}

	AttributeHeaderLong* a = allocateAttrLong( attrTagIndexToTag( tagIndex ) );
	if ( !a )
		return nullptr;

	a->tagIndex_ = static_cast<u8>( tagIndex );
	a->attrType_ = typ;
	a->arraySize_ = 255;
	a->offsetToNextAttribute_ = 0;
	a->arraySizeLong_ = arraySize;
	a->offsetToNextAttributeLong_ = 0;
	if ( curAttribute_ )
	{
		AttributeHeader* ca = getAttribute( curAttribute_ );
		if ( ca->arraySize_ == 255 || ca->attrType_ == AttributeType::DataWithLayout )
			reinterpret_cast<AttributeHeaderLong*>( ca )->offsetToNextAttributeLong_ = getAttrOffsetLong( a, ca );
		else
			ca->offsetToNextAttribute_ = getAttrOffset( a, ca );
	}
	curAttribute_ = getOffsetRelativeToStreamStart( a );
	++getCurNode()->nAttributes_;
	return a;
}

void OutputStreamImpl::pushStack( size_t nOffset )
{
	if ( stackCount_ >= eStackDepth )
	{
		error( Error::hierarchyOverflow, 0, "hierarchy is too deep (max depth is %d)", eStackDepth );
		return;
	}

	nodeStack_[stackCount_] = nOffset;
	if ( curNode_ && getNode( curNode_ )->nChildren_ == 0 )
		prevSiblingStack_[stackCount_] = 0;
	curNode_ = nOffset;
	++stackCount_;
}

void OutputStreamImpl::popStack()
{
	if ( stackCount_ == 0 )
	{
		error( Error::hierarchyCorrupted, 0, errorPushPop );
		return;
	}

	--stackCount_;
	nodeStack_[stackCount_] = 0;
	if ( stackCount_ > 0 )
		curNode_ = nodeStack_[stackCount_ - 1];
	else
		curNode_ = 0;
}

void OutputStreamImpl::begin( const char magic[8] )
{
	// stream header
	StreamHeader* header = allocateMem<StreamHeader>( 0 );
	if ( !header )
		return;

	memcpy( header->magic_, magic, 8 );

	// root node
	rootImpl_ = getOffsetRelativeToStreamStart( addNode( MakeTag( "root" ) ) );
	pushStack( rootImpl_ );
}

void OutputStreamImpl::end()
{
	popStack();

	if ( error_ )
		return;

	if ( stackCount_ != 0 )
		error( Error::hierarchyCorrupted, 0, errorPushPop );

	u8* attrTagRemapTable = allocateMemImpl( nAttrTag_ * sizeof( TagType ), alignof( TagType ), 0 );
	HISTREAM_ASSERT( validateAlign( attrTagRemapTable, alignof( TagType ) ) );
	memcpy( attrTagRemapTable, attrTag_, nAttrTag_ * sizeof( TagType ) );

	StreamHeader* header = reinterpret_cast<StreamHeader*>( buf_ );
	header->offsetToTagRemapTable_ = (u32)( (size_t)attrTagRemapTable - (size_t)buf_ );
	header->nEntriesInTagRemapTable_ = nAttrTag_;
}

void OutputStreamImpl::pushChild( TagType tag )
{
	NodeHeader* n = addNode( tag );
	if ( !n )
		return;

	if ( prevSiblingStack_[stackCount_] )
	{
		NodeHeader* prevSibling = getNode( prevSiblingStack_[stackCount_] );
		size_t o = (size_t)n - (size_t)prevSibling;
		if ( o > std::numeric_limits<NodeOffsetType>::max() )
		{
			error( Error::dataOverflow, 0, errorNodeOverflow );
			return;
		}
		prevSibling->offsetToNextSibling_ = static_cast<NodeOffsetType>( o );
	}

	prevSiblingStack_[stackCount_] = getOffsetRelativeToStreamStart( n );

	if ( curNode_ )
	{
		NodeHeader* cn = getNode( curNode_ );
		if ( cn->nChildren_ == 0 )
		{
			size_t o = (size_t)n - (size_t)cn;
			if ( o > std::numeric_limits<NodeOffsetType>::max() )
			{
				error( Error::dataOverflow, 0, errorNodeOverflow );
				return;
			}
			cn->offsetToFirstChild_ = static_cast<NodeOffsetType>( o );
		}
		++cn->nChildren_;
	}

	pushStack( getOffsetRelativeToStreamStart( n ) );
	curAttribute_ = 0;
}

void OutputStreamImpl::popChild()
{
	popStack();
	curAttribute_ = 0;
}

} // namespace _private

} // namespace HiStream
