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
#include "data_info.h"
#include "debug.h"
#include <errno.h>
#include <assert.h>

DataInfo::DataInfo(const std::string &key, const std::string &keyVersion, const std::string &data):
        key(key), keyVersion(keyVersion), d(data), updates()
{
}

DataInfo::~DataInfo()
{
    if(this->hasUpdates())
        hflat_warning("Deleting data info structure containing updates.");
}

void DataInfo::mergeDataChanges(std::string fresh)
{
    fresh.resize(std::max(fresh.size(), d.size()));
    for (auto& update : updates){
        if(update.second)
            fresh.replace(update.first, update.second, d, update.first, update.second);
        else
            fresh.resize(update.first);
    }
    assert(fresh.size() <= 1024 * 1024);
    d = fresh;
}

int DataInfo::updateData(const char *data, off_t offset, size_t size)
{
    assert(offset + size <= 1024 * 1024);
    d.resize(std::max((size_t) offset + size, d.size()));
    try {
         d.replace(offset, size, data, size);
    } catch (std::exception& e) {
        hflat_warning("Exception thrown trying to replace byte range [%d,%d] in string Reason: %s", offset, size, e.what());
        return -EINVAL;
    }

    this->updates.push_back(std::pair<off_t, size_t>(offset, size));
    return 0;
}

void DataInfo::truncate(off_t offset)
{
    assert(offset <= 1024 * 1024);
    hflat_trace("Resizing data info %s to %d bytes.",key.data(),offset);
    d.resize(offset);
    this->updates.push_back(std::pair<off_t, size_t>(offset, 0));
}

bool DataInfo::hasUpdates() const
{
    return !updates.empty();
}

void DataInfo::forgetUpdates()
{
    updates.clear();
}

const std::string& DataInfo::getKeyVersion() const
{
    return keyVersion;
}

void DataInfo::setKeyVersion(const std::string &v)
{
    keyVersion=v;
}

const std::string& DataInfo::data() const
{
    return d;
}

const std::string & DataInfo::getKey() const
{
    return key;
}
