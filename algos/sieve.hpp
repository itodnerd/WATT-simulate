//
// Created by dev on 06.10.21.
//

#include "EvictStrategy.hpp"
using clock_list_type = std::pair<PID, bool>;

// Unordered map<PID> and search min;
struct sieve: public EvictStrategyHashList<clock_list_type> {
    using upper = EvictStrategyHashList<clock_list_type>;
    // We use an inverted List (fresh elements in back, old ones in front)
    std::list<clock_list_type>::iterator pointer;
    sieve(): upper() {
    }
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        pointer = ram.begin();
    }
    virtual typename std::list<clock_list_type>::iterator insertElement(const Access& access, std::list<clock_list_type>& ram){
        ram.push_back({access.pid, false});
        return ram.begin();
    }
    typename std::list<clock_list_type>::iterator updateElement(typename std::list<clock_list_type>::iterator old, const Access& access, std::list<T>& ram){
        old->second = true;
        return old;
    }


    PID evictOne(Access) override{
        while(true){
            if (pointer == ram.end()){
                pointer = ram.begin();
            }
            if (pointer->second == false){
                auto me = pointer;
                PID removed = me->first;
                pointer++;
                fast_finder.erase(removed);
                upper::ram.erase(me);


            }
            pointer->second=false;
            pointer++;
        }
    }


};

