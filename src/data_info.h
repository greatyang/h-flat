#ifndef DATA_INFO_H
#define DATA_INFO_H
#include <string>
#include <list>
#include "vector_clock.h"

class DataInfo final {
private:
    std::string   key;
    VectorClock   keyVersion;

    std::string d;                      // the actual data
    std::list<std::pair<off_t, size_t> > updates;     // a list of bit-regions that have been changed since this data block has last been flushed

public:
    int  updateData(const char *data, off_t offset, size_t size);
    bool hasUpdates() const;
    void forgetUpdates();
    void mergeDataChanges(std::string fresh);

    const std::string& data() const;
    const std::string& getKey() const;
    const VectorClock &getKeyVersion() const;
    void setKeyVersion(const VectorClock &vc);

public:
    explicit DataInfo(std::string key, const VectorClock &keyVersion, std::string data);
    ~DataInfo();
};

#endif
