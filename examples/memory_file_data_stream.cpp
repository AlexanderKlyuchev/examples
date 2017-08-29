#include <stdexcept>
#include <cstdio>

#include "filesystem/memory_file_data_stream.h"

namespace  sq { namespace filesystem {

SQ_BEGIN_OBJECT(MemoryDataStream)
SQ_END_OBJECT()


MemoryFileId::MemoryFileId(const std::string& fname, FileMode access_mode_param)
{
    fileId = new fileIdPlatform();
    fileId->initData(fname, access_mode_param);
}

MemoryFileId::~MemoryFileId()
{
    if (fileId)
        delete fileId;
}

fileIdPlatform* MemoryFileId::getFileId() const
{
    return fileId;
}

MemoryFileId::MemoryFileId(const MemoryFileId& other)
{
    fileId = new fileIdPlatform(*other.getFileId());
}

bool MemoryFileId::operator==(const MemoryFileId& other) const
{
    return *fileId == *other.getFileId();
}

bool MemoryFileId::operator<(const MemoryFileId& other) const
{
    return  *getFileId() < *other.getFileId();
}


size_t MemoryDataStream::write(uint8_t* buffer, size_t size)
{

    size_t amount = std::min(size, filesize - curPos);
    if (amount)
    {
        uint8_t* ptr = static_cast<uint8_t*>(mappedView);
        memcpy(ptr + curPos, buffer, amount);
    }

    curPos += amount;
    return amount;
}

/// seek to absolute position if from_current==false else to relative from current
bool MemoryDataStream::seek(std::streamoff offset, bool fromCurrent)
{
    if (!fromCurrent)
    {
        if (offset < filesize)
        {
            curPos = offset;
            return true;
        }
    }
    else
    {
        if (curPos + offset < filesize)
        {
            curPos += offset;
        }
    }

    return false;

}

void MemoryDataStream::addRef()
{
    ++referenceCount;
}

bool MemoryDataStream::hasRef()
{
    return referenceCount > 0;
}

bool MemoryDataStream::eof()
{
    return curPos >= filesize;
}

size_t MemoryDataStream::tell()
{
    return curPos;
};


std::shared_ptr<DataStream> MemoryDataStream::clone()
{
    throw std::logic_error("Can't clone memory mapped file");
};

MemoryDataStream::sptr MemoryDataStream::open(const std::string& fn, FileMode mode, size_t dataSize)
{
    auto stream = std::make_shared<MemoryDataStream>(fn, mode, dataSize);
    return stream->isValid() ? stream : nullptr;
}

MemoryDataStream::MemoryDataStream()
{
}

MemoryDataStream::~MemoryDataStream()
{
	close();
	deletePlatformFields();
}

MemoryDataStream::MemoryDataStream(const std::string& fname, FileMode accessModeParam, size_t dataSize) :
	filename(fname),
	filesize(0),
	hint(CacheHint::Normal),
	mappedBytes(0),
	curPos(0),
	referenceCount(1),
	mappedView(nullptr)
{

	initPlatformFields();
	initFileOptions(accessModeParam);
	fileOpen();

	filesize = getFileSize();

	// initial mapping
	size_t bytesToMap = filesize;
	if (bytesToMap < dataSize)
		bytesToMap = dataSize;

	remap(0, bytesToMap);

    filesize = getFileSize();
}


void MemoryDataStream::close()
{
    if (--referenceCount > 0)
    {
        return;
    }

	closeMappedFile();
    mappedView = nullptr;
    filesize = 0;
}

uint8_t MemoryDataStream::operator[](size_t offset) const
{
    return static_cast<uint8_t*>(mappedView)[offset];
}

uint8_t MemoryDataStream::at(size_t offset) const
{
    if (!mappedView)
        throw std::invalid_argument("No view mapped");
    if (offset >= filesize)
        throw std::out_of_range("View is not large enough");

    return operator[](offset);
}

/// raw access
const uint8_t* MemoryDataStream::getData() const
{
    return static_cast<const uint8_t*>(mappedView);
}

/// read `size` bytes to buffer, return realy readed bytes
size_t MemoryDataStream::read(uint8_t* buffer, size_t size)
{
    size_t amount = std::min(size, filesize - curPos);
    if(amount)
    {
        uint8_t * ptr = static_cast<uint8_t*>(mappedView);
        memcpy(buffer, ptr + curPos, amount);
    }

    curPos += amount;
    return amount;
}

/// true, if file successfully opened
bool MemoryDataStream::isValid() const
{
    return mappedView != nullptr;
}

size_t MemoryDataStream::getSize()
{
    return filesize;
}

/// get number of actually mapped bytes
size_t MemoryDataStream::mappedSize() const
{
    return mappedBytes;
}

// replace mapping by a new one of the same file, offset MUST be a multiple of the page size
bool MemoryDataStream::remap(uint64_t offset, size_t bytesToMap)
{
    if (mappedView)
    {
        memUnmap();
        mappedView = nullptr;
    }

    // don't go further than end of file
    if (offset > filesize)
        return false;
    //if (offset + bytesToMap > filesize)
    //    bytesToMap = size_t(filesize - offset);

    mappedView = memMap(bytesToMap, offset);

	if (mappedView)
	{
		mappedBytes = bytesToMap - offset;
	}

    return mappedView ? true : false;
}


MemoryFilePool::sptr MemoryFilePool::_instance = nullptr;

MemoryFilePool::sptr MemoryFilePool::instance()
{
    if( !_instance )
    {
        _instance = MemoryFilePool::sptr(new MemoryFilePool());
    }

    return _instance;
}
MemoryDataStream::sptr MemoryFilePool::openFile(const std::string& fname, FileMode mode)
{
    MemoryFileId id(fname, mode);

    auto it = theMap.find(id);
    if (it != theMap.end())
    {
        it->second->addRef();
        return it->second;
    }
    else
    {
        MemoryDataStream::sptr file;
        file = MemoryDataStream::open(fname, mode);
        theMap.emplace(id, file);
        return file;
    }
}

void MemoryFilePool::closeFile(const std::string& fname, FileMode mode)
{
    MemoryFileId id(fname, mode);

    auto it = theMap.find(id);
    if (it != theMap.end())
    {
        it->second->close();
        if (!it->second->hasRef()) {
            theMap.erase(it);
        }
    }
    else
    {
        throw std::invalid_argument("File "+fname+" not found in pool");
    }
}

}}