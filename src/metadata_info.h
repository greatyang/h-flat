/* In-memory metadata representation. */
#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_
#include "metadata.pb.h"
#include "data_info.h"
#include <map>

class MetadataInfo final
{
    std::string       systemPath;       // key in flat namespace where metadata is stored
    std::int64_t      currentVersion;   // current version of metadata key
    posixok::Metadata md_const;         // exact copy of on-drive metadata with version currentVersion
    posixok::Metadata md_mutable;       // metadata structure containing local changes not yet written to drive

    std::map<std::uint32_t, DataInfo> datablocks;    // in-memory data blocks

public:
    explicit MetadataInfo();
    explicit MetadataInfo(const std::string &key);
    ~MetadataInfo();


public:
    bool                mergeMD(const posixok::Metadata & md, std::int64_t version);
    void                setMD(const posixok::Metadata & md, std::int64_t version);
    posixok::Metadata & getMD();
    void                setSystemPath(const std::string &key);
    const std::string & getSystemPath();
    void                setCurrentVersion(std::int64_t version); // after metadata version increased due to a successful put
    std::int64_t        getCurrentVersion();

    // returns false if no in-memory data info structure exists for the supplied block number
    DataInfo *getDataInfo(std::uint32_t block_number);
    void      setDataInfo(std::uint32_t block_number, const DataInfo &di);

    // convenience functions to update time-stamps according to current local clock
    void updateACMtime();
    void updateACtime();

    // returns 'true' if changed, 'false' if unchanged
    bool computePathPermissionChildren();
};

#endif /* METADATA_INFO_H_ */
