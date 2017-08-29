#ifdef _MSC_VER

#include <windows.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filesystem/memory_file_data_stream.h"

namespace sq { namespace filesystem {

struct memMapPlatformFields
{
    void* file;
    void* mappedFile;
    int fileOpenMode = 0;
    int sharedMode = 0;
    int prot = 0;
    int mmapMode = 0;
    int protectionMode = 0;
};

struct fileIdPlatformFields
{
    FileMode accessMode;
    _dev_t dev;
    _ino_t ino;
};

fileIdPlatform::fileIdPlatform(const fileIdPlatform& other)
{
    fields = std::unique_ptr<fileIdPlatformFields>(new fileIdPlatformFields());
    fields->dev = other.fields->dev;
    fields->ino = other.fields->ino;
    fields->accessMode = other.fields->accessMode;
}

void fileIdPlatform::initData(const std::string& fname, FileMode access_mode_param)
{
    struct _stat buf;
    int result;
    result = _stat(fname.c_str(), &buf);

    if (result)
    {
        std::stringstream err_str;
        switch (errno)
        {
            case ENOENT:
            err_str << "File " << fname << " not found." << std::endl;
            break;
            case EINVAL:
            err_str << "Invalid parameter to _stat." << std::endl;
            break;
            default:
            err_str << "Unexpected error in _stat."<< std::endl;
        }

        throw std::invalid_argument("can't stat file: " + err_str.str());
    }
    else
    {
        fields->dev = buf.st_dev;
        fields->ino = buf.st_ino;
        fields->accessMode = access_mode_param;
    }
}


bool fileIdPlatform::operator==(const fileIdPlatform& other) const
{
    return fields->dev == other.fields->dev &&
           fields->ino == other.fields->ino &&
           fields->accessMode == other.fields->accessMode;
}

bool fileIdPlatform::operator<(const fileIdPlatform& other) const
{
    return (fields->ino < other.fields->ino) ||
           (fields->ino == other.fields->ino && fields->accessMode < other.fields->accessMode) ||
           (fields->ino == other.fields->ino && fields->accessMode == other.fields->accessMode &&
           fields->dev < other.fields->dev);
}

fileIdPlatform::fileIdPlatform()
{
    fields = std::unique_ptr<fileIdPlatformFields>(new fileIdPlatformFields());
}

fileIdPlatform::~fileIdPlatform()
{
}

void MemoryDataStream::initPlatformFields()
{
	mmPlatformFields = new memMapPlatformFields();
	//mmPlatformFields = std::make_shared<memMapPlatformFields>();
}

void MemoryDataStream::deletePlatformFields()
{
	if (mmPlatformFields)
	{
		delete mmPlatformFields;
		mmPlatformFields = nullptr;
	}
	//mmPlatformFields = std::make_shared<memMapPlatformFields>();
}

bool MemoryDataStream::save()
{
    int filesize = getFileSize();
    return ::FlushViewOfFile(mappedView, filesize);
}

size_t MemoryDataStream::getFileSize()
{
    LARGE_INTEGER result;
    if (!GetFileSizeEx(mmPlatformFields->file, &result))
        return 0;
    return static_cast<size_t>(result.QuadPart);
}

void MemoryDataStream::closeMappedFile()
{
    if(mappedView)
    {
        ::UnmapViewOfFile(mappedView);
    }

    if (mmPlatformFields->mappedFile)
    {
        ::CloseHandle(mmPlatformFields->mappedFile);
		mmPlatformFields->mappedFile = nullptr;
    }

    if(mmPlatformFields->file)
    {
        ::CloseHandle(mmPlatformFields->file);
		mmPlatformFields->file = nullptr;
    }
}

void MemoryDataStream::memUnmap()
{
    ::UnmapViewOfFile(mappedView);
}

void* MemoryDataStream::memMap(size_t& bytesToMap, uint64_t offset)
{
    if(!mmPlatformFields->file)
        return nullptr;

    if (mmPlatformFields->mappedFile)
    {
        ::CloseHandle(mmPlatformFields->mappedFile);
		mmPlatformFields->mappedFile = nullptr;
    }

	mmPlatformFields->mappedFile = ::CreateFileMapping(mmPlatformFields->file, nullptr, mmPlatformFields->protectionMode, offset, bytesToMap, nullptr);

    if (!mmPlatformFields->mappedFile)
    {
        return nullptr;
    }

    uint32_t offsetLow  = uint32_t(offset & 0xFFFFFFFF);
    uint32_t offsetHigh = uint32_t(offset >> 32);

    // get memory address
    void* mappedView = ::MapViewOfFile(mmPlatformFields->mappedFile, mmPlatformFields->mmapMode, offsetHigh, offsetLow, bytesToMap);

    if (mappedView == nullptr)
    {
		#if MM_DEBUG
		wchar_t buf[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
		std::cout << buf << std::endl;
		#endif
        bytesToMap = 0;
        return nullptr;
    }

    return mappedView;
}


int MemoryDataStream::getPageSize()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwAllocationGranularity;
}

void MemoryDataStream::fileOpen()
{
    uint32_t winHint = 0;
    switch (hint)
    {
        case CacheHint::Normal:
            winHint = FILE_ATTRIBUTE_NORMAL;
            break;
	    case CacheHint::SequentialScan:
            winHint = FILE_FLAG_SEQUENTIAL_SCAN;
            break;
	    case CacheHint::RandomAccess:
            winHint = FILE_FLAG_RANDOM_ACCESS;
            break;
        default:
            winHint = FILE_ATTRIBUTE_NORMAL;
            break;
    }

	mmPlatformFields->file = ::CreateFile(filename.c_str(), mmPlatformFields->fileOpenMode, mmPlatformFields->sharedMode, nullptr, mmPlatformFields->prot, winHint, nullptr);
    if (mmPlatformFields->file == INVALID_HANDLE_VALUE)
    {
        #if MM_DEBUG
        wchar_t buf[256];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
		std::cout << buf << std::endl;
        #endif
        return;
    }

}

void MemoryDataStream::initFileOptions(FileMode accessModeParam)
{
    switch (accessModeParam)
    {
        case FileMode::READ:
			mmPlatformFields->fileOpenMode = GENERIC_READ;
			mmPlatformFields->sharedMode = FILE_SHARE_READ;
			mmPlatformFields->prot = OPEN_EXISTING;
			mmPlatformFields->mmapMode = FILE_MAP_READ;
			mmPlatformFields->protectionMode = PAGE_READONLY;
            break;
        case FileMode::READ_WRITE:
			mmPlatformFields->fileOpenMode = GENERIC_READ | GENERIC_WRITE;
			mmPlatformFields->sharedMode = 0;
			mmPlatformFields->prot = OPEN_ALWAYS;
			mmPlatformFields->mmapMode = FILE_MAP_WRITE | FILE_MAP_READ;
			mmPlatformFields->protectionMode = PAGE_READWRITE;
            break;
        case FileMode::WRITE:
			mmPlatformFields->fileOpenMode = GENERIC_WRITE;
			mmPlatformFields->sharedMode = FILE_SHARE_WRITE;
			mmPlatformFields->prot = OPEN_ALWAYS;
			mmPlatformFields->mmapMode = FILE_MAP_WRITE;
			mmPlatformFields->protectionMode = PAGE_READWRITE;
        case FileMode::APPEND:
			mmPlatformFields->fileOpenMode = GENERIC_READ | GENERIC_WRITE;
			mmPlatformFields->sharedMode = 0;
			mmPlatformFields->prot = OPEN_EXISTING;
			mmPlatformFields->mmapMode = FILE_MAP_WRITE;
			mmPlatformFields->protectionMode = PAGE_READWRITE;
            break;
        default:
            break;
    }
}

}}

#endif