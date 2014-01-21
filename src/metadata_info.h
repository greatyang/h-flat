/* In-memory metadata representation. */
#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_
#include "metadata.pb.h"
#include "data_info.h"
#include <map>

class MetadataInfo
    final {
        posixok::Metadata md;
        std::string systemPath;           // key in flat namespace where metadata is stored
        std::int64_t currentVersion;      // current version of metadata key in flat namespace
        std::map<std::uint32_t, DataInfo> datablocks;   // all read-in data blocks

    public:
        explicit MetadataInfo();
        explicit MetadataInfo(const std::string &key);
        ~MetadataInfo();

    public:
        // direct access to protobuf structure
        posixok::Metadata * pbuf();

        // convenience functions to update timestamps in protobuf
        void updateACMtime();
        void updateACtime();

        // contains all the path permission computation, returns 'true' if changed, 'false' if unchanged.
        bool computePathPermissionChildren();

        // merge local changes with the supplied metadata structure
        int mergeMetadataChanges(const posixok::Metadata * const fresh);

        bool hasDataInfo(std::uint32_t block_number);
        DataInfo *getDataInfo(std::uint32_t block_number);
        void setDataInfo(std::uint32_t block_number, const DataInfo &di);
        void forgetDataInfo(std::uint32_t block_number);

        const std::string &getSystemPath();
        void setSystemPath(const std::string &systemPath);

        std::int64_t getCurrentVersion();
        void setCurrentVersion(std::int64_t version);
    };

#endif /* METADATA_INFO_H_ */
