#pragma once

#include "data_stream.h"
#include <string>

namespace sb { namespace filesystem {

        #define ALIGN_TO_PAGE(x) ((x) & ~(MemoryDataStream::getPageSize() - 1))
        #define UPPER_ALIGN_TO_PAGE(x) ALIGN_TO_PAGE((x)+(MemoryDataStream::getPageSize()-1))
        #define OFFSET_INTO_PAGE(x) ((x) & (MemoryDataStream::getPageSize() - 1))

        enum class allocator_flags {
            MAP_WHOLE_FILE = 1,
            ALLOW_REMAP = 2,
            BYPASS_FILE_POOL = 4,
            KEEP_FOREVER = 8
        };

        struct memMapPlatformFields;

        enum class CacheHint
        {
            Normal,         ///< good overall performance
            SequentialScan, ///< read file only once with few seeks
            RandomAccess    ///< jump around
        };

        struct fileIdPlatformFields;

        struct fileIdPlatform
        {
            fileIdPlatform();
            fileIdPlatform(const fileIdPlatform& other);
            void initData(const std::string& fname, FileMode access_mode_param);
            virtual ~fileIdPlatform();
            std::unique_ptr<fileIdPlatformFields> fields;

            bool operator==(const fileIdPlatform& other) const;
            bool operator<(const fileIdPlatform& other) const;
        };

        class MemoryFileId {
        public:
            MemoryFileId();
            virtual ~MemoryFileId();
            MemoryFileId(const MemoryFileId& other);
            MemoryFileId(const std::string& fname, FileMode mode);
            bool operator==(const MemoryFileId& other) const;
            bool operator<(const MemoryFileId& other) const;
            fileIdPlatform* getFileId() const;
        private:
            fileIdPlatform* fileId;
        };

        class MemoryDataStream : public DataStream
        {
            SQ_DECLARE_OBJECT(MemoryDataStream)
            friend class MemoryFilePool;
        public:
            using sptr = std::shared_ptr<MemoryDataStream>;

			MemoryDataStream();
            MemoryDataStream(const std::string& filename, FileMode accessModeParam, size_t dataSize = 0 );
            ~MemoryDataStream();
            /// read `size` bytes to buffer, return realy readed bytes
            virtual size_t read(uint8_t* buffer, size_t size) override;
            /// seek to absolute position if from_current==false else to relative from current
            virtual size_t write(uint8_t* buffer, size_t size) override;
            /// seek to absolute position if from_current==false else to relative from current
            virtual bool seek(std::streamoff offset, bool fromCurrent) override;
            /// eof indicator
            virtual bool eof() override;
            /// path information
            virtual const std::string& path() const override { return filename; }

            static MemoryDataStream::sptr open(const std::string& fn, FileMode mode, size_t dataSize = 0);

            /// get current position
            virtual size_t tell() override;
            virtual void close() override;
            virtual size_t getSize()  override;
            std::shared_ptr<DataStream> clone() override;
            virtual bool isValid() const override;
            size_t  mappedSize() const;
            const   uint8_t* getData() const;
            bool    save();

            /// access position, no range checking (faster)
            unsigned char operator[](size_t offset) const;
            unsigned char at(size_t offset) const;

			size_t getFileSize();
			void   memUnmap();
			void*  memMap(size_t& bytesToMap, uint64_t offset);
			int    getPageSize();
			void   fileOpen();
			void   initFileOptions(FileMode accessModeParam);
			void   initPlatformFields();
			void   deletePlatformFields();
			void   closeMappedFile();
        private:

            std::string filename;
            // file size
            size_t  filesize;
            // caching strategy
            CacheHint   hint;
            // mapped size
            size_t  mappedBytes;
            size_t curPos;
            int referenceCount;

            memMapPlatformFields* mmPlatformFields;
            void* mappedView;

            bool remap(uint64_t offset, size_t mappedBytes);
            void addRef();
            bool hasRef();
        };

        class MemoryFilePool {
        public:
            using map = std::map<MemoryFileId, MemoryDataStream::sptr> ;
            using sptr = std::shared_ptr<MemoryFilePool>;

            static sptr instance();

            MemoryDataStream::sptr openFile(const std::string& fname, FileMode access_mode);
            void closeFile(const std::string& fname, FileMode access_mode);

        private:
            MemoryFilePool(){};

        private:
            map theMap;
            static sptr _instance;
        };

    }}
