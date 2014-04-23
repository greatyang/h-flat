#include "data_info.h"
#include "debug.h"
#include <errno.h>
#include <assert.h>

DataInfo::DataInfo(const std::string &key, const std::string &keyVersion, const std::string &data):
        key(key), keyVersion(keyVersion), d(data)
{
}

DataInfo::~DataInfo()
{
    if(this->hasUpdates())
        hflat_warning("Deleting data info structure containing updates.");
}

void DataInfo::mergeDataChanges(std::string fresh)
{
    hflat_trace("merging");
    fresh.resize(std::max(fresh.size(), d.size()));
    for (auto& update : updates){
        if(update.second)
            fresh.replace(update.first, update.second, d);
        else
            fresh.resize(update.first);
    }
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
    hflat_trace("Resizing data info %s to %d bytes.",key.data(),offset);
    d.resize(offset);
    this->updates.push_back(std::pair<off_t, size_t>(offset, 0));
}

bool DataInfo::hasUpdates() const
{
    return !this->updates.empty();
}

void DataInfo::forgetUpdates()
{
    this->updates.clear();
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
    return this->d;
}

const std::string & DataInfo::getKey() const
{
    return this->key;
}
