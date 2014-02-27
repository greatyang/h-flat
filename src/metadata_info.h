/* In-memory metadata representation. */
#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_
#include "metadata.pb.h"
#include "data_info.h"
#include <memory>
#include <map>


class MetadataInfo final
{
    std::string       systemPath;       // key in flat namespace where metadata is stored
    std::int64_t      currentVersion;   // current version of metadata key
    std::shared_ptr<DataInfo> dirty_data;

    posixok::Metadata md_const;         // exact copy of on-drive metadata with version currentVersion
    posixok::Metadata md_mutable;       // metadata structure containing local changes not yet written to drive

public:
    explicit MetadataInfo();
    explicit MetadataInfo(const std::string &key);
    ~MetadataInfo();

public:
    void                setMD(const posixok::Metadata & md, std::int64_t version);
    posixok::Metadata & getMD();
    void                setSystemPath(const std::string &key);
    const std::string & getSystemPath();
    void                setCurrentVersion(std::int64_t version); // after metadata version increased due to a successful put
    std::int64_t        getCurrentVersion();

    // convenience functions to update time-stamps according to current local clock
    void updateACMtime();
    void updateACtime();

    // returns 'true' if changed, 'false' if unchanged
    bool computePathPermissionChildren();

    bool setDirtyData(std::shared_ptr<DataInfo>& di);
    std::shared_ptr<DataInfo>& getDirtyData();

};

#endif /* METADATA_INFO_H_ */
