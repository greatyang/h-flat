/* In-memory metadata representation. */
#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_
#include "metadata.pb.h"
#include "data_info.h"
#include "vector_clock.h"
#include <memory>
#include <map>

class MetadataInfo final
{
private:
    std::string       systemPath;       // key in flat namespace where metadata is stored
    VectorClock       keyVersion;

    posixok::Metadata md;               // metadata structure
    std::shared_ptr<DataInfo> dirty_data; // reference to data block updated by a write call, compare data.cc


public:
    explicit MetadataInfo(const std::string &key);
    ~MetadataInfo();

public:
    void                setMD(const posixok::Metadata & md, const VectorClock &version);
    posixok::Metadata & getMD();
    void                setSystemPath(const std::string &key);
    const std::string & getSystemPath() const;
    void                setKeyVersion(const VectorClock &vc);
    const VectorClock & getKeyVersion() const;


    // convenience functions to update time-stamps according to current local clock
    void updateACMtime();
    void updateACtime();

    // returns 'true' if changed, 'false' if unchanged
    bool computePathPermissionChildren();

    bool setDirtyData(std::shared_ptr<DataInfo>& di);
    std::shared_ptr<DataInfo>& getDirtyData();
};

#endif /* METADATA_INFO_H_ */
