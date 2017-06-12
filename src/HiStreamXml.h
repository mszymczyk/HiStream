#pragma once

#include "HiStream.h"

namespace HiStream
{

namespace XmlConvertError
{
enum Type
{
	noError,
	xmlOpenError, // error while loading xml file
	tagError, // xml 'tag' attribute is missing
	typeError, // xml 'type' attribute (or one of dependent xml attributes) is missing or has incorrect type
	layoutError, // 0 or more than 255 layouts
	alignError, // output stream is incorrectly aligned
	valueError, // value is empty, array/string length mismatch or value out of range
	outputStreamError, // histream is broken
};
} // namespace XmlConvertError

struct HiStreamBuffer
{
public:
	~HiStreamBuffer();

	HiStreamBuffer( HiStreamBuffer&& other ) noexcept;
	HiStreamBuffer& operator=( HiStreamBuffer&& other ) noexcept;

	const u8* data() const { return data_; }
	size_t dataSize() const { return dataSize_; }
	const char* text() const { return reinterpret_cast<const char*>( data_ ); }
	size_t textSize() const { return dataSize_; }

private:
	HiStreamBuffer( u8* bin, size_t binSize, Allocator& alloc, XmlConvertError::Type convertError, Error::Type hisError );

	HiStreamBuffer( const HiStreamBuffer& other ) = delete;
	HiStreamBuffer& operator=( const HiStreamBuffer& other ) = delete;

private:
	u8* data_ = nullptr;
	size_t dataSize_ = 0;
	Allocator alloc_;

	XmlConvertError::Type convertError_ = XmlConvertError::noError;
	Error::Type hisError_ = Error::noError;

	friend HiStreamBuffer convertHisToXml( const u8* binData, const size_t binDataSize, Allocator* alloc, Logger* log );
	friend HiStreamBuffer convertXmlToHis( const char* text, size_t textSize, Allocator* alloc, Logger* log );
};

// pugixml is used for xml conversion internally and it doesn't support memory allocator 'per document', only global one
// that's why we can't do multiple simultaneous conversions when using different allocator for each document
// if each conversion uses the same allocator, then multiple simultaneous conversions are ok
HiStreamBuffer convertHisToXml( const u8* binData, const size_t binDataSize, Allocator* alloc = nullptr, Logger* log = nullptr );
HiStreamBuffer convertXmlToHis( const char* text, size_t textSize, Allocator* alloc = nullptr, Logger* log = nullptr );

} // namespace HiStream
