#pragma once

namespace HiStream
{

inline Error::Type OutputStream::error() const
{
	return impl_.error_;
}

inline const char* OutputStream::errorStr() const
{
	return Error::ToString( impl_.error_ );
}

inline void OutputStream::addString( TagType tag, const char* str )
{
	addString( tag, str, strlen( str ) );
}

inline void OutputStream::addStringU8( TagType tag, const char* str, u8 x )
{
	addStringU8( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringS8( TagType tag, const char* str, s8 x )
{
	addStringS8( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringU16( TagType tag, const char* str, u16 x )
{
	addStringU16( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringS16( TagType tag, const char* str, s16 x )
{
	addStringS16( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringU32( TagType tag, const char* str, u32 x )
{
	addStringU32( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringS32( TagType tag, const char* str, s32 x )
{
	addStringS32( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringU64( TagType tag, const char* str, u64 x )
{
	addStringU64( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringS64( TagType tag, const char* str, s64 x )
{
	addStringS64( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringFloat( TagType tag, const char* str, float x )
{
	addStringFloat( tag, str, strlen( str ), x );
}

inline void OutputStream::addStringDouble( TagType tag, const char* str, double x )
{
	addStringDouble( tag, str, strlen( str ), x );
}

inline void* OutputStream::addDataWithLayout( TagType tag, std::initializer_list<DataLayoutElement> layout, const void* data, u32 dataSize, u32 alignment )
{
	return addDataWithLayout( tag, layout.begin(), layout.size(), data, dataSize, alignment );
}



inline Node::Node()
	: node_( nullptr )
{	}

inline TagType Node::tag() const
{
	return node_->tag_;
}




inline u32 Node::numChildren() const
{
	return node_->nChildren_;
}

inline NodeIterator Node::childrenBegin() const
{
	const u8* base = reinterpret_cast<const u8*>( node_ );
	const _private::NodeHeader* firstChild = reinterpret_cast<const _private::NodeHeader*>( base + node_->offsetToFirstChild_ );
	return NodeIterator( Node( firstChild, is_ ), 0 );
}

inline NodeIterator Node::childrenEnd() const
{
	return NodeIterator( Node(), node_->nChildren_ );
}

inline ObjectRange<NodeIterator> Node::children() const
{
	return ObjectRange<NodeIterator>( childrenBegin(), childrenEnd() );
}




inline u32 Node::numAttributes() const
{
	return node_->nAttributes_;
}

inline AttributeIterator Node::attributesBegin() const
{
	const _private::AttributeHeader* firstAttribute = reinterpret_cast<const _private::AttributeHeader*>( node_ + 1 );
	return AttributeIterator( Attribute( firstAttribute, is_ ), 0 );
}
inline AttributeIterator Node::attributesEnd() const
{
	return AttributeIterator( Attribute(), node_->nAttributes_ );
}

inline ObjectRange<AttributeIterator> Node::attributes() const
{
	return ObjectRange<AttributeIterator>( attributesBegin(), attributesEnd() );
}

inline Node::Node( const _private::NodeHeader* node, const _private::InputStreamImpl* is )
	: node_( node )
	, is_( is )
{	}



inline AttributeType::Type Attribute::type() const
{
	return attr_->attrType_;
}

inline HiStream::TagType Attribute::tag() const
{
	return is_->tagIndexToType( attr_->tagIndex_ );
}

inline u32 Attribute::arrayLength() const
{
	return attr_->arraySize_ != 255 ? attr_->arraySize_ : reinterpret_cast<const _private::AttributeHeaderLong*>( attr_ )->arraySizeLong_;
}

inline Attribute::Attribute()
	: attr_( nullptr ), is_( nullptr )
{	}

inline Attribute::Attribute( const _private::AttributeHeader* attr, const _private::InputStreamImpl* is )
	: attr_( attr )
	, is_( is )
{	}


inline InputStreamHeader::InputStreamHeader( const u8* buf, size_t bufSize )
	: impl_( (bufSize >= sizeof( _private::StreamHeader ) ) ? *reinterpret_cast<const _private::StreamHeader*>( buf ) : _private::StreamHeader() )
{	}

inline const char* InputStreamHeader::getMagic() const
{
	return reinterpret_cast<const char*>( impl_.magic_ );
}


inline InputStream::InputStream( const u8* buf, size_t bufSize )
	: impl_( buf, bufSize )
{
	HISTREAM_ASSERT( bufSize >= 16 + sizeof( _private::NodeHeader ) );
}


inline Node InputStream::getRoot() const
{
	return Node( impl_.rootNode_, &impl_ );
}




inline bool NodeIterator::operator==( const NodeIterator& rhs ) const
{
	return childIndex_ == rhs.childIndex_;
}

inline bool NodeIterator::operator!=( const NodeIterator& rhs ) const
{
	return childIndex_ != rhs.childIndex_;
}

inline const Node& NodeIterator::operator*() const
{
	return node_;
}

inline const NodeIterator& NodeIterator::operator++()
{
	const u8* base = reinterpret_cast<const u8*>( node_.node_ );
	node_.node_ = reinterpret_cast<const _private::NodeHeader*>( base + node_.node_->offsetToNextSibling_ );
	++childIndex_;
	return *this;
}

inline NodeIterator::NodeIterator( const Node& node, u32 childIndex )
	: node_( node )
	, childIndex_( childIndex )
{	}



inline bool AttributeIterator::operator==( const AttributeIterator& rhs ) const
{
	return attrIndex_ == rhs.attrIndex_;
}

inline bool AttributeIterator::operator!=( const AttributeIterator& rhs ) const
{
	return attrIndex_ != rhs.attrIndex_;
}

inline const Attribute& AttributeIterator::operator*() const
{
	return attr_;
}

inline const Attribute* AttributeIterator::operator->() const
{
	return &attr_;
}

inline const AttributeIterator& AttributeIterator::operator++()
{
	const u8* base = reinterpret_cast<const u8*>( attr_.attr_ );
	u32 offsetToNext;
	if ( attr_.attr_->arraySize_ == 255 || attr_.attr_->attrType_ == AttributeType::DataWithLayout )
		offsetToNext = reinterpret_cast<const _private::AttributeHeaderLong*>( attr_.attr_ )->offsetToNextAttributeLong_;
	else
		offsetToNext = attr_.attr_->offsetToNextAttribute_;
	attr_.attr_ = reinterpret_cast<const _private::AttributeHeader*>( base + offsetToNext );
	++attrIndex_;
	return *this;
}

inline AttributeIterator::AttributeIterator( const Attribute& attr, u32 attrIndex )
	: attr_( attr )
	, attrIndex_( attrIndex )
{	}

} // namespace HiStream
