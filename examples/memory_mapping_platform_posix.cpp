#ifndef _MSC_VER

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "filesystem/memory_file_data_stream.h"

namespace sq { namespace filesystem {

        struct memMapPlatformFields
        {
            int file;
            int fileOpenMode = 0;
            int mmapMode = 0;
            int prot = 0;
        };

        struct fileIdPlatformFields
        {
            FileMode accessMode;
            dev_t device;
            ino_t inode;
        };

        fileIdPlatform::fileIdPlatform()
        {
            fields = std::unique_ptr<fileIdPlatformFields>(new fileIdPlatformFields());
        }

        fileIdPlatform::~fileIdPlatform()
        {
        }


        fileIdPlatform::fileIdPlatform(const fileIdPlatform& other)
        {
            fields = std::unique_ptr<fileIdPlatformFields>(new fileIdPlatformFields());
            fields->device = other.fields->device;
            fields->inode = other.fields->inode;
            fields->accessMode = other.fields->accessMode;
        }

        void fileIdPlatform::initData(const std::string& fname, FileMode access_mode_param)
        {
            struct stat buf;
            if (stat(fname.c_str(), &buf) < 0)
            {
                throw std::invalid_argument("Cannot stat file " + fname);
            }

            fields->device = buf.st_dev;
            fields->inode = buf.st_ino;
            fields->accessMode = access_mode_param;
        }


        bool fileIdPlatform::operator==(const fileIdPlatform& other) const
        {
            return fields->device == other.fields->device &&
                   fields->inode == other.fields->inode &&
                   fields->accessMode == other.fields->accessMode;
        }

        bool fileIdPlatform::operator<(const fileIdPlatform& other) const
        {
            return fields->inode < other.fields->inode ||
            (fields->inode == other.fields->inode && fields->accessMode < other.fields->accessMode) ||
            (fields->inode == other.fields->inode && fields->accessMode == other.fields->accessMode
             && fields->device < other.fields->device);
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
            return (msync(mappedView, filesize, MS_SYNC) != -1);
        }

        size_t MemoryDataStream::getFileSize()
        {
            struct stat buf;
            if (fstat(mmPlatformFields->file, &buf) < 0)
            {
                return 0;
            }
            return buf.st_size;
        }


		void MemoryDataStream::closeMappedFile()
        {
            if (mappedView)
            {
                ::munmap(mappedView, filesize);
            }

            if (mmPlatformFields->file)
            {
                ::close(mmPlatformFields->file);
            }

			mmPlatformFields->file = 0;
        }

        void MemoryDataStream::memUnmap()
        {
            ::munmap(mappedView, mappedBytes);
        }

        void* MemoryDataStream::memMap(size_t& bytesToMap, uint64_t offset)
        {
            if (!mmPlatformFields->file)
                return nullptr;


            if (bytesToMap > filesize && mmPlatformFields->fileOpenMode != O_RDONLY)
            {
                if (lseek(mmPlatformFields->file, bytesToMap - 1, SEEK_SET) < 0)
                {
                    ::close(mmPlatformFields->file);
                    return nullptr;
                }

                if (!filesize && ::write(mmPlatformFields->file, "", 1) < 0)
                {
                    ::close(mmPlatformFields->file);
                    return nullptr;
                }

            }

            void *mappedView = mmap(nullptr, bytesToMap, mmPlatformFields->prot, mmPlatformFields->mmapMode, mmPlatformFields->file, offset);

            if (mappedView == MAP_FAILED)
            {
                bytesToMap = 0;
                return nullptr;
            }

            auto linuxHint = 0;
            switch (hint)
            {
                case CacheHint::Normal:
                    linuxHint = MADV_NORMAL;
                    break;
                case CacheHint::SequentialScan:
                    linuxHint = MADV_SEQUENTIAL;
                    break;
                case CacheHint::RandomAccess:
                    linuxHint = MADV_RANDOM;
                    break;
                default:
                    break;
            }
            // assume that file will be accessed soon
            //linuxHint |= MADV_WILLNEED;
            // assume that file will be large
            //linuxHint |= MADV_HUGEPAGE;

            ::madvise(mappedView, bytesToMap, linuxHint);

            return mappedView;

        }

        int MemoryDataStream::getPageSize()
        {
            return sysconf(_SC_PAGESIZE);
        }



        void MemoryDataStream::fileOpen()
        {
			mmPlatformFields->file = ::open(filename.c_str(), mmPlatformFields->fileOpenMode, (mode_t) 0600);
            if (mmPlatformFields->file < 0)
            {
                ::close(mmPlatformFields->file);
                return;
            }

        }

        void MemoryDataStream::initFileOptions(FileMode accessModeParam)
        {
            switch (accessModeParam)
            {
                case FileMode::READ:
					mmPlatformFields->prot = PROT_READ;
					mmPlatformFields->mmapMode |= MAP_SHARED;
					mmPlatformFields->fileOpenMode = O_RDONLY;
                    break;
                case FileMode::READ_WRITE:
					mmPlatformFields->prot = PROT_READ | PROT_WRITE;
					mmPlatformFields->mmapMode |= MAP_SHARED;
					mmPlatformFields->fileOpenMode = O_RDWR | O_CREAT;
                    break;
                case FileMode::WRITE:
                    /*APPEND MODE TBD*/
                case FileMode::APPEND:
					mmPlatformFields->prot = PROT_WRITE;
					mmPlatformFields->mmapMode |= MAP_SHARED;
					mmPlatformFields->fileOpenMode = O_WRONLY | O_CREAT;
                    break;
                default:
                    break;
            }
        }

    }}

#endif