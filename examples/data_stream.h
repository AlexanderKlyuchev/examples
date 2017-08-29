#pragma once

#include "common.h"
#include "scripting/TypeInfo.h"

namespace sq { namespace filesystem {

enum class FileMode
{
	READ,
	WRITE,
	READ_WRITE,
    APPEND
};

class DataStream : public sq::common::noncopyable
{
	SQ_DECLARE_BASE_OBJECT(DataStream)
public:
	virtual ~DataStream() {}
	/// read `size` bytes to buffer, return realy readed bytes
	virtual size_t read(unsigned char *buffer, size_t size) = 0;
	/// write `size` bytes from buffer, return realy writed bytes
	virtual size_t write(unsigned char *buffer, size_t size) = 0;
	/// seek to absolute position if from_current==false else to relative from current
	virtual bool seek(std::streamoff offset, bool fromCurrent) = 0;
	/// eof indicator
	virtual bool eof() = 0;
    virtual bool isValid() const = 0;
	/// path ( used in handling of include directives )
	virtual const std::string& path() const = 0;
	virtual void close() = 0;
	/// get current position
	virtual size_t tell() = 0;
	/// get all stream size ( if applicable )
	virtual size_t getSize() { return 0; }
	/// clone stream ( open some file )
	virtual std::shared_ptr<DataStream> clone() { return std::shared_ptr<DataStream>(); }

    using sptr = std::shared_ptr<DataStream>;
};

class StreamLineReader : public sq::common::noncopyable
{
public:
	explicit StreamLineReader(const DataStream::sptr &stream)
		:stream(stream), bufferPos(0) {}
	/// read line from stream
	void getLine(std::string& to);
	bool eof();

	const DataStream::sptr &getStream() const { return stream; }

	static inline std::shared_ptr<StreamLineReader> make(const DataStream::sptr &stream)
	{
		return std::make_shared<StreamLineReader>(stream);
	}
public:
private:
	DataStream::sptr stream;
	typedef std::vector<unsigned char> bufferType;
	bufferType buffer;
	size_t bufferPos;
	static const size_t BUFFER_SIZE = 1024 * 4;
};

}}

