#ifndef DATA_INFO_H
#define DATA_INFO_H
#include <string>
#include <list>

class DataInfo final {
private:
    std::string   key;
    std::int64_t  currentVersion;       // current version of data key in flat namespace
    std::string d;                      // the actual data

    // a list of bit-regions that have been changed since this data block has last been flushed
    std::list<std::pair<off_t, size_t> > updates;

public:
    explicit DataInfo(std::string key, std::int64_t currentVersion, std::string data);
    ~DataInfo();

    int  updateData(const char *data, off_t offset, size_t size);
    bool hasUpdates();
    void forgetUpdates();
    void mergeDataChanges(std::string fresh);

    const std::string& data();
    const std::string& getKey();
    std::int64_t getCurrentVersion();
    void setCurrentVersion(std::int64_t version);
};

#endif
