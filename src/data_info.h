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
#ifndef DATA_INFO_H
#define DATA_INFO_H
#include <string>
#include <list>

class DataInfo final {
private:
    std::string   key;
    std::string   keyVersion;

    std::string d;                                    // the actual data
    std::list<std::pair<off_t, size_t> > updates;     // a list of bit-regions that have been changed since this data block has last been flushed

public:
    int  updateData(const char *data, off_t offset, size_t size);
    void truncate(off_t offset);
    bool hasUpdates() const;
    void forgetUpdates();
    void mergeDataChanges(std::string fresh);

    const std::string& data() const;
    const std::string& getKey() const;
    const std::string& getKeyVersion() const;
    void setKeyVersion(const std::string& v);

public:
    explicit DataInfo(const std::string &key, const std::string &keyVersion, const std::string &data);
    ~DataInfo();
};

#endif
