#include "data_info.h"
#include "debug.h"
#include <errno.h>
#include <assert.h>

DataInfo::DataInfo(std::string data, std::int64_t version) :
        d(data), currentVersion(version)
{
}

DataInfo::DataInfo() :
        d("empty"), currentVersion(0)
{
}

DataInfo::~DataInfo()
{
}

void DataInfo::mergeDataChanges(std::string fresh)
{
    fresh.resize(std::max(fresh.size(), d.size()));
    for (auto& update : updates)
        fresh.replace(update.first, update.second, d);
    d = fresh;
}

int DataInfo::updateData(const char *data, off_t offset, size_t size)
{
    assert(offset + size <= 1024 * 1024);

    d.resize(std::max((size_t) offset + size, d.size()));
    try {
        d.replace(offset, size, data);
    } catch (std::exception& e) {
        pok_warning("Exception thrown trying to replace byte range [%d,%d] in string Reason: %s", offset, size, e.what());
        return -EINVAL;
    }

    this->updates.push_back(std::pair<off_t, size_t>(offset, size));
    return 0;
}

bool DataInfo::hasUpdates()
{
    return !this->updates.empty();
}

void DataInfo::forgetUpdates()
{
    this->updates.clear();
}

std::int64_t DataInfo::getCurrentVersion()
{
    return this->currentVersion;
}

void DataInfo::setCurrentVersion(std::int64_t version)
{
    this->currentVersion = version;
}

const std::string& DataInfo::data()
{
    return this->d;
}
