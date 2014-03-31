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
