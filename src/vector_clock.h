#ifndef VECTOR_CLOCK_H_
#define VECTOR_CLOCK_H_
#include <uuid/uuid.h>
#include <assert.h>
#include <list>

/* Vector Clock using a series of uuid values to define order for up to 'capacity' time-points. */
class VectorClock final
{
private:
    std::list<std::string> _clock;

public:
    const std::list<std::string> & data() const{
        return _clock;
    }
    std::string serialize() const{
        std::string value;
        for(auto element : _clock)
            value+=element;
        return value;
    }
    VectorClock& operator++(){
        uuid_t uuid;
        uuid_generate(uuid);
        _clock.push_front(std::string(reinterpret_cast<const char *>(uuid), sizeof(uuid_t)));
        _clock.pop_back();
        return *this;
    }

    explicit VectorClock (unsigned int capacity = 4):
        _clock( capacity, std::string(sizeof(uuid_t),'0') ){
    }
    explicit VectorClock (const std::string &serialized){
        assert(serialized.size() % sizeof(uuid_t) == 0);
        for(size_t i=0; i<serialized.size(); i+=sizeof(uuid_t))
            _clock.push_back(serialized.substr(i, sizeof(uuid_t)));
    }
    ~VectorClock(){};
};


/* Comparison operators. Note that semantics don't match arithmetic semantics exactly:
 * If clocks don't share any uuid all comparisons (with the exception of !=) will evaluate to false. This means that from
 * a<b == false   it doesn't follow that    a>=b == true */
inline bool operator==(const VectorClock &a, const VectorClock &b){ return a.data().front().compare(b.data().front()) == 0; }
inline bool operator!=(const VectorClock &a, const VectorClock &b){ return !operator==(a,b); }
inline bool operator< (const VectorClock &a, const VectorClock &b){
    auto iterb = b.data().begin();
    while( ++iterb != b.data().end() )
        if(a.data().front().compare(*iterb) == 0) return true;
    return false;
}
inline bool operator> (const VectorClock &a, const VectorClock &b){ return operator<(b,a); }
inline bool operator<=(const VectorClock &a, const VectorClock &b){ return operator<(a,b) || operator==(a,b); }
inline bool operator>=(const VectorClock &a, const VectorClock &b){ return operator>(a,b) || operator==(a,b); }


#endif /* VECTOR_CLOCK_H_ */
