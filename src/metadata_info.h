/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_
#include "metadata.pb.h"
#include "data_info.h"
#include <memory>
#include <map>

class MetadataInfo final
{
private:
    std::string       systemPath;       // key in flat namespace where metadata is stored
    std::string       keyVersion;
    hflat::Metadata md;               // metadata structure

    // write aggregation support
    std::shared_ptr<DataInfo> dirty_data; // reference to data block updated by a write call compare data.cc

public:
    explicit MetadataInfo(const std::string &key);
    ~MetadataInfo();

public:
    void                setMD(const hflat::Metadata& md, const std::string& version);
    hflat::Metadata & getMD();
    void                setSystemPath(const std::string &key);
    const std::string & getSystemPath() const;
    void                setKeyVersion(const std::string &version);
    const std::string & getKeyVersion() const;


    // convenience functions to update time-stamps according to current local clock
    void updateACMtime();
    void updateACtime();

    // returns 'true' if changed, 'false' if unchanged
    bool computePathPermissionChildren();

    bool setDirtyData(std::shared_ptr<DataInfo>& di);
    std::shared_ptr<DataInfo>& getDirtyData();
};

#endif /* METADATA_INFO_H_ */
