//
// Created by dev on 06.10.21.
//
#pragma once

#ifndef C_EVICTSTRATEGY_HPP
#define C_EVICTSTRATEGY_HPP

#endif //C_EVICTSTRATEGY_HPP

#include <map>
#include <list>
#include <unordered_set>
#include <random>
#include <cassert>
#include <unordered_map>
#include "general.hpp"

static bool keepFirst(const std::list<RefTime>& l, const std::list<RefTime>& r) {
    if(l.size()== r.size()){
        return *(l.rbegin()) < *(r.rbegin()); // higher is younger
    }else{
        return l.size() < r.size(); // bigger is better
    }
};

static bool keepFirst(const std::list<std::pair<RefTime, bool>>& l, const std::list<std::pair<RefTime, bool>>& r) {
    if(l.size()== r.size()){
        return (l.rbegin())->first < (r.rbegin())->first; // higher is younger
    }else{
        return l.size() < r.size(); // bigger is better
    }
};

static uint calc_hist_size(RamSize ram_size, int Z){
    return (uint) (Z >= 0 ? ram_size * Z : ( Z == -1 ? UINT32_MAX : ram_size / (-Z)));
}

class EvictStrategy
{
public:
    explicit EvictStrategy(){};

    virtual void evaluateRamList(const std::vector<Access> &data, ramListType &ramList,
                                 rwListSubType &readWriteList) {
        for(auto& ram_size: ramList){
            readWriteList[ram_size] = evaluateOne(data, ram_size);
        }
    }
    std::pair<uint, uint> evaluateOne(const std::vector<Access> &data, RamSize ram_size){
        reInit(ram_size);
        checkConditions(ram_size);
        return executeStrategy(data);
    }
protected:
    /**
     * Resets everything back to start
     * @param ram_size Size of Ram in this iteration
     */
    virtual void reInit(RamSize ram_size){
        RAM_SIZE = ram_size;
        dirty_in_ram.clear();
        in_ram.clear();
        curr_count=0;
    }
    /**
     * Check if everything was initiated correctly
     * @param ram_size
     */
    void checkConditions(RamSize ram_size){
        assert(RAM_SIZE == ram_size);
        assert(dirty_in_ram.empty());
        assert(in_ram.empty());
        assert(curr_count == 0);
    }
    /**
     * Simulates the access once
     * @param access_data list of accesses
     * @return
     */
    std::pair<uint, uint> executeStrategy(const std::vector<Access>& access_data){
        uint page_misses = 0, dirty_evicts = 0;
        for(const Access& single_access: access_data){
            checkSizes(single_access.pid);
            if(!in_ram[single_access.pid]){
                page_misses++;
                if(curr_count >= RAM_SIZE){
                    dirty_evicts += evict(single_access.pos);
                }
                curr_count ++;
            }
            access(single_access);
            dirty_in_ram[single_access.pid] = dirty_in_ram[single_access.pid] || single_access.write;
            in_ram[single_access.pid] = true;
        }
        return std::pair(page_misses, dirty_evicts + dirtyPages());
    }

    RamSize RAM_SIZE=0, curr_count=0;
    std::vector<bool> dirty_in_ram;
    std::vector<bool> in_ram;

    /**
     * Handle the access in the internal structure
     * @param access
     */
    virtual void access(const Access& access) = 0;
    /**
     * Handle one evict iteration.
     * Per default handels oneEviction
     *
     * Per Eviction postRemove has to be called and the returne values have to be added together
     * @param time
     * @return sum(map(lambda PID x: postRemove(x), evictions))
     */
    virtual uint evict(RefTime time) {
        return postRemove(evictOne(time));
    }
    /**
     * Handle one eviction, easiest version
     * @param curr_time
     * @return PID of page to evict
     */
    virtual PID evictOne(RefTime curr_time) = 0;

    /**
     * Checks ALL pages, if they are dirty and in ram (slow)
     * @return
     */
    int dirtyPages(){
        return std::count(dirty_in_ram.begin(), dirty_in_ram.end(), true);
    }

    /**
     * Validates, that vectors are big enough
     * @param pid
     */
    void checkSizes(PID pid){
        if(dirty_in_ram.size() <= pid){
            dirty_in_ram.resize(pid+1, false);
        }
        if(in_ram.size() <= pid){
            in_ram.resize(pid+1, false);
        }
    }
    /**
     * handles eviction of one page.
     * changes structures
     * @param pid
     * @return
     */
    uint postRemove(PID pid){
        in_ram[pid]=false;
        curr_count--;
        if (dirty_in_ram[pid]){
            dirty_in_ram[pid] = false;
            return 1;
        }
        return 0;
    }
};

/**
 * Here we can use one container for storage of information for the pages
 * @tparam Container
 */
template<class Container>
class EvictStrategyContainer: public EvictStrategy{
public:
    EvictStrategyContainer(): EvictStrategy(){}
protected:
    Container ram;
    void reInit(RamSize ram_size) override{
        EvictStrategy::reInit(ram_size);
        ram.clear();
    }

    static bool compare_second(const std::pair<int, int>& l, const std::pair<int, int>& r) { return l.second < r.second; };
};

/**
 * A list with hashmap for the list.
 * Per default it saves the PID and evicts by LRU
 * @tparam Container
 */
template<class T>
class EvictStrategyHashList: public EvictStrategyContainer<std::list<T>>{
using upper = EvictStrategyContainer<std::list<T>>;
public:
    EvictStrategyHashList(): upper() {}
protected:
    std::unordered_map<PID, typename std::list<T>::iterator> fast_finder;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        fast_finder.clear();
    }
    void access(const Access& access) override{
        if(upper::in_ram[access.pid]){
            fast_finder[access.pid] = updateElement(fast_finder[access.pid], access);

        }else{
            fast_finder[access.pid] = insertElement(access);
        }
    };
    virtual PID evictOne(RefTime) override{
        typename std::list<T>::iterator min = upper::ram.begin();
        PID pid = *min;
        fast_finder.erase(pid);
        upper::ram.erase(min);
        return pid;
    }

    /**
     * Algorithm specific Insert Function
     * @param access
     * @return Iterator for pos in RAM
     */
    virtual typename std::list<T>::iterator insertElement(const Access& access){
        upper::ram.push_back(access.pid);
        return std::prev(upper::ram.end());
    }
    /**
     * Algorithm specific update Function
     * @param old current position in RAM
     * @param access
     * @return Iterator for pos in RAM
     */
    virtual typename std::list<T>::iterator updateElement(typename std::list<T>::iterator old, const Access& access){
        upper::ram.erase(old);

        upper::ram.push_back(access.pid);
        return std::prev(upper::ram.end());
    }

};

template<class T>
class EvictStrategyHashVector: public EvictStrategyContainer<std::vector<T>>{
    using upper = EvictStrategyContainer<std::vector<T>>;
public:
    EvictStrategyHashVector(): upper() {}
protected:
    std::unordered_map<PID, uint> fast_finder;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        upper::ram.resize(ram_size);
        fast_finder.clear();
    }
    void access(const Access& access) override{
        if(upper::in_ram[access.pid]){
            updateElement(fast_finder[access.pid], access);

        }else{
            fast_finder[access.pid] = insertElement(access);
        }
    };

    /**
     * Algorithm specific Insert Function
     * @param access
     * @return pos in RAM
     */
    virtual uint insertElement(const Access& access) = 0;

    /**
     * Algorithm specific update Function
     * @param old current position in RAM
     * @param access
     * @return pos in RAM
     */
    virtual void updateElement(uint old, const Access& access) = 0;

};

class EvictStrategyHistory: public EvictStrategyContainer<std::unordered_map<PID, std::list<RefTime>>>{
public:
    using container_type = std::unordered_map<PID, std::list<RefTime>>;
    using upper = EvictStrategyContainer<container_type>;
    EvictStrategyHistory(int K): upper(), K(K) {}
protected:
    uint K;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
    }
    void access(const Access& access) override{
        std::   list<RefTime>& hist = ram[access.pid];
        hist.push_front(access.pos);
        if(hist.size() > K){
            hist.resize(K);
        }
        assert(*hist.begin() == access.pos);
    };
    PID evictOne(RefTime curr_time) override{
        PID pid = chooseEviction(curr_time);
        ram.erase(pid);
        return pid;
    }

    virtual PID chooseEviction(RefTime){
        container_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        while(runner!= ram.end()){
            if(keepFirst(runner->second, ram[candidate])){
                candidate = runner->first;
            }
            ++runner;
        }
        return candidate;
    }
};

template<class history_type>
class EvictStrategyKeepHistory: public EvictStrategyContainer<std::unordered_map<PID, history_type>>{
protected:
    using ram_type = std::unordered_map<PID, history_type>;
    using upper = EvictStrategyContainer<ram_type>;
public:
    EvictStrategyKeepHistory(int Z=0): upper(), Z(Z){}
protected:
    const int Z;
    uint hist_size;
    std::unordered_map<PID, std::pair<std::list<PID>::iterator, history_type>> out_of_mem_history;
    std::list<PID> out_of_mem_order;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        out_of_mem_history.clear();
        out_of_mem_order.clear();
        hist_size = calc_hist_size(ram_size, Z);
    }
    void access(const Access& access) override{
        // Load out_of_mem_values (if exists)
        if(!upper::in_ram[access.pid]){
            auto old_value = out_of_mem_history.find(access.pid);
            if(old_value!= out_of_mem_history.end()){
                out_of_mem_order.erase(old_value->second.first);
                upper::ram[access.pid] = old_value->second.second;
                out_of_mem_history.erase(old_value);
            }
        }
        changeElement(access);
    };
    virtual void changeElement(const Access access) =0;

    virtual PID evictOne(RefTime curr_time) override{
        PID candidate = chooseEviction(curr_time);

        handle_out_of_ram(candidate);

        upper::ram.erase(candidate);
        return candidate;
    }

    virtual PID chooseEviction(RefTime)= 0;

    void handle_out_of_ram(PID candidate){
        out_of_mem_order.push_front(candidate);
        auto& element = out_of_mem_history[candidate];
        element.first = out_of_mem_order.begin();
        element.second = std::move(upper::ram[candidate]);
        while(out_of_mem_order.size() > hist_size){
            PID last = out_of_mem_order.back();
            out_of_mem_history.erase(last);
            out_of_mem_order.pop_back();
        }
    }

    static void push_frontAndResize(const Access& access, std::list<RefTime>& hist, uint K, uint epoch_size = 1) {
        RefTime current = access.pos / epoch_size;
        if(!hist.empty() && *hist.begin() == current){
            // already logged in this epoch;
            return;
        }
        hist.push_front(current);
        if(hist.size() > K){
            hist.resize(K);
        }
    };
    static void push_frontAndResize(const Access& access, std::list<std::pair<RefTime, bool>>& hist, uint K, uint epoch_size = 1) {
        RefTime current = access.pos / epoch_size;
        if(!hist.empty() && hist.begin()->first == current){
            // already logged in this epoch;
            hist.begin()->second = hist.begin()->second || access.write;
            return;
        }
        hist.push_front({current, access.write});
        if(hist.size() > K){
            hist.resize(K);
        }
    };

};

class EvictStrategyKeepHistoryOneList: public EvictStrategyKeepHistory<std::list<RefTime>>{
    using history_type = std::list<RefTime>;
    using upper = EvictStrategyKeepHistory<history_type>;
public:
    EvictStrategyKeepHistoryOneList(int K, int Z): upper(Z), K(K){}
protected:
    uint K;

    void changeElement(const Access access) override {
        push_frontAndResize(access, ram[access.pid], K);
    }

    virtual PID chooseEviction(RefTime) override{
        ram_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        while(runner!= upper::ram.end()){
            if(keepFirst(runner->second, ram[candidate])){
                candidate = runner->first;
            }
            ++runner;
        }// */
        return candidate;
    }
};

class EvictStrategyKeepHistoryCombined: public EvictStrategyKeepHistory<std::list<std::pair<RefTime, bool>>>{
protected:
    using history_type = std::list<std::pair<RefTime, bool>>;
    using upper = EvictStrategyKeepHistory<history_type>;
public:
    EvictStrategyKeepHistoryCombined(uint K=1, int Z=0, uint epoch_size=1): upper(Z), K(K), epoch_size(epoch_size){}
protected:
    const uint K, epoch_size;
    uint epoch_size_iter;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        if(epoch_size == 0){
            epoch_size_iter = 1;
        }
        else{
            epoch_size_iter = ram_size/epoch_size;
            if(epoch_size_iter < 1)
                epoch_size_iter = 1;
        }

    }
    void changeElement(const Access access) override {
        // Push to according list;
        push_frontAndResize(
                access,
                ram[access.pid],
                K,
                epoch_size_iter);
        // if is write and write is logged as read: push to readList
    };

    virtual PID chooseEviction(RefTime) override{
        ram_type::iterator runner = ram.begin();
        PID candidate = runner->first;

        while(runner!= upper::ram.end()){
            if(keepFirst(runner->second, ram[candidate])){
                candidate = runner->first;
            }
            ++runner;
        }
        return candidate;
    }

};

// First list is for reads, second for writes
class EvictStrategyKeepHistoryReadWrite: public EvictStrategyKeepHistory< std::pair<std::list<RefTime>, std::list<RefTime>>>{
protected:
    using history_type = std::pair<std::list<RefTime>, std::list<RefTime>>;
    using ram_type = std::unordered_map<PID, history_type>;
    using upper = EvictStrategyKeepHistory<history_type>;
public:
    EvictStrategyKeepHistoryReadWrite(uint KR=1, uint KW=1, int Z=0, bool write_as_read=false, uint epoch_size = 1): upper(Z), K_R(KR), K_W(KW), write_as_read(write_as_read), epoch_size(epoch_size){}
protected:
    const uint K_R, K_W;
    const bool write_as_read; // write counts as read?
    const uint epoch_size;
    uint  epoch_size_iter;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        if(epoch_size == 0)epoch_size_iter = 1;
        else if((epoch_size_iter = ram_size / epoch_size) < 1) epoch_size_iter = 1;

    }
    void changeElement(const Access access) override {
        // Push to according list;
        push_frontAndResize(
                access,
                access.write ? ram[access.pid].second : ram[access.pid].first,
                access.write ? K_W: K_R,
                epoch_size_iter);
        // if is write and write is logged as read: push to readList
        if(write_as_read && access.write){
            push_frontAndResize(access, ram[access.pid].first, K_R, epoch_size_iter);
        }
    };

        virtual PID chooseEviction(RefTime) override{
        ram_type::iterator runner = ram.begin();
        PID candidate = runner->first;

        while(runner!= upper::ram.end()){
            if(keepFirst(runner->second.first, ram[candidate].first)){
                candidate = runner->first;
            }
            ++runner;
        }
        return candidate;
    }

};