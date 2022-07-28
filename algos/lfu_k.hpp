//
// Created by dev on 06.10.21.
//
#include "EvictStrategy.hpp"
#include <functional>
double get_frequency(std::vector<RefTime>& candidate, RefTime curr_time, float first_value=1.0);
double get_frequency(std::vector<std::pair<RefTime, bool>>& candidate, RefTime curr_time, uint write_cost);

struct LFU_K: public EvictStrategyHistory{
    using upper = EvictStrategyHistory;
    LFU_K(int K): upper(K) {}

    PID chooseEviction(RefTime curr_time) override{
        container_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        double candidate_freq = get_frequency(runner->second, curr_time);
        while(runner!= ram.end()){
            double runner_freq = get_frequency(runner->second, curr_time);
            if(runner_freq < candidate_freq){
                candidate = runner->first;
                candidate_freq = runner_freq;
            }
            ++runner;
        }
        return candidate;
    }
};

struct LFU_K_Z: public EvictStrategyKeepHistoryOneList{
    using upper = EvictStrategyKeepHistoryOneList;
    using container_type = EvictStrategyHistory::container_type;
    LFU_K_Z(int K, int Z): upper(K,Z) {}

    PID chooseEviction(RefTime curr_time) override{
        container_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        double candidate_freq = get_frequency(runner->second, curr_time);
        ++runner;
        while(runner!= ram.end()){
            double runner_freq = get_frequency(runner->second, curr_time);
            if(runner_freq <= candidate_freq){
                candidate = runner->first;
                candidate_freq = runner_freq;
            }
            ++runner;
        }
        return candidate;
    }
};

struct LFU2_K_Z: public EvictStrategyKeepHistoryReadWrite{
    using upper = EvictStrategyKeepHistoryReadWrite;
    LFU2_K_Z(int K, int Z): upper(K, 0, Z) {}

    PID chooseEviction(RefTime curr_time)  override{
        ram_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        double candidate_freq = get_frequency(runner->second.first, curr_time);
        while(runner!= ram.end()){
            double runner_freq = get_frequency(runner->second.first, curr_time);
            if(runner_freq < candidate_freq){
                candidate = runner->first;
                candidate_freq = runner_freq;
            }
            ++runner;
        }
        return candidate;
    }

};

struct LFU_2K_Z: public EvictStrategyKeepHistoryReadWrite{
    using upper = EvictStrategyKeepHistoryReadWrite;

    LFU_2K_Z(uint KR, uint KW, int Z, bool write_as_read): upper(KR, KW, Z, write_as_read){}

    PID chooseEviction(RefTime curr_time) override{
        ram_type::iterator runner = ram.begin();
        PID candidate = runner->first;
        double candidate_freq = eval_freq(runner, curr_time);
        while(runner!= ram.end()){
            double runner_freq = eval_freq(runner, curr_time);
            if(runner_freq < candidate_freq){
                candidate = runner->first;
                candidate_freq = runner_freq;
            }
            ++runner;
        }
        return candidate;
    }
    double eval_freq(ram_type::iterator candidate, RefTime curr_time){
        double candidate_freq_R = get_frequency(candidate->second.first, curr_time);
        double candidate_freq_W = get_frequency(candidate->second.second, curr_time);
        double candidate_freq = candidate_freq_R + candidate_freq_W;
        if(!write_as_read){
            candidate_freq += candidate_freq_W;
        }
        return candidate_freq;
    }

};

struct LFU_2K_Z_rand: public EvictStrategyKeepHistoryReadWrite{
    using upper = EvictStrategyKeepHistoryReadWrite;

    LFU_2K_Z_rand(uint KR, uint KW, int Z, uint randSize, bool write_as_read):
        upper(KR, KW, Z, write_as_read),
        randSize(randSize){}

    const uint randSize;

    uint rand_list_length;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        rand_list_length = calculate_rand_list_length(ram_size, randSize);
    }

    PID evictOne(Access access) override{
        std::vector<ram_type::iterator> elements = getElementsFromRam<ram_type::iterator>(rand_list_length);

        PID evict = chooseEvictionLOCAL(access.pos, elements);

        handle_out_of_ram(evict);

        ram.erase(evict);
        return evict;
    }

    PID chooseEvictionLOCAL(RefTime curr_time, std::vector<ram_type::iterator> elements){
        std::vector<ram_type::iterator>::iterator runner = elements.begin();
        double candidate_freq = eval_freq(*runner, curr_time);
        PID evict = 0;
        while(runner!= elements.end()){
            double runner_freq = eval_freq(*runner, curr_time);
            if(runner_freq < candidate_freq){
                evict = (*runner)->first;
                candidate_freq = runner_freq;
            }
            ++runner;
        }
        return evict;
    }
    double eval_freq(ram_type::iterator candidate, RefTime curr_time){
        double candidate_freq_R = get_frequency(candidate->second.first, curr_time);
        double candidate_freq_W = get_frequency(candidate->second.second, curr_time);
        double candidate_freq = candidate_freq_R + candidate_freq_W;
        if(!write_as_read){
            candidate_freq += candidate_freq_W;
        }
        return candidate_freq;
    }

};

struct LFU_2K_E_real: public EvictStrategyKeepHistoryReadWrite{
    using upper = EvictStrategyKeepHistoryReadWrite;

    LFU_2K_E_real(uint KR, uint KW, uint randSize, uint randSelector = 1, bool write_as_read = true,
                  uint epoch_size = 1, uint write_cost = 1, float first_value = 1.0) :
            upper(KR, KW, -1, write_as_read, epoch_size),
            randSelector(randSelector), // how many do we want to evict?
            randSize(randSize), // how many are evaluated
            writeCost(write_cost),
            first_value(first_value){}

    const uint randSelector, randSize, writeCost;
    const float first_value;

    uint rand_list_length;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        rand_list_length = calculate_rand_list_length(ram_size, randSize);
    }
    uint evict(Access access) override{
        RefTime curr_time = access.pos / epoch_size_iter;
        std::vector<ram_type::iterator> elements = getElementsFromRam<ram_type::iterator>(rand_list_length);

        // Sort elements by frequency; //std::min_element
        auto comperator = gt_compare_freq(curr_time, this->write_as_read, this->writeCost, first_value);
        std::make_heap(elements.begin(), elements.end(), comperator);

        uint dirtyEvicts = 0;
        for(uint i = 0; i< randSelector && !elements.empty(); i++){
            std::pop_heap(elements.begin(), elements.end(), comperator);
            ram_type::iterator element = elements.back();
            PID pid = element->first;
            elements.pop_back();
            handle_out_of_ram(pid);
            ram.erase(element);
            dirtyEvicts += postRemove(pid);
        }
        return dirtyEvicts;
    }

    static double
    eval_freq(ram_type::iterator& candidate, RefTime curr_time, bool write_as_read, uint write_cost = 1, float first_value = 1.0) {
        double candidate_freq_R = get_frequency(candidate->second.first, curr_time, first_value);
        double candidate_freq_W = get_frequency(candidate->second.second, curr_time, first_value);
        double candidate_freq = candidate_freq_R + candidate_freq_W * write_cost;
        if(!write_as_read){
            candidate_freq += candidate_freq_W;
        }
        return candidate_freq;
    }


    static std::function<double(ram_type::iterator &, ram_type::iterator &)>
    gt_compare_freq(RefTime curr_time, bool write_as_read, uint write_cost, float first_value) {
        return [curr_time, write_as_read, write_cost, first_value](ram_type::iterator& l, ram_type::iterator& r) {
            return eval_freq(l, curr_time, write_as_read, write_cost, first_value)
            > eval_freq(r, curr_time, write_as_read, write_cost, first_value);
        };
    };


};

struct LFU_2K_E_real_ver2: public EvictStrategyKeepHistoryReadWrite{
    using upper = EvictStrategyKeepHistoryReadWrite;

    LFU_2K_E_real_ver2(uint KR, uint KW, uint randSize, uint randSelector = 1, bool write_as_read = true,
                  uint epoch_size = 1, uint write_cost = 1) :
            upper(KR, KW, -1, write_as_read, epoch_size),
            randSelector(randSelector), // how many do we want to evict?
            randSize(randSize), // how many are evaluated
            writeCost(write_cost){}

    const uint randSelector, randSize, writeCost;

    uint rand_list_length;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        rand_list_length = calculate_rand_list_length(ram_size, randSize);
    }
    uint evict(Access access) override{
        RefTime curr_time = access.pos / epoch_size_iter;
        std::vector<ram_type::iterator> elements = getElementsFromRam<ram_type::iterator>(rand_list_length);

        // Sort elements by frequency; //std::min_element
        auto comperator = gt_compare_freq(curr_time, this->write_as_read, this->writeCost,
                                          this->dirty_in_ram);
        std::make_heap(elements.begin(), elements.end(), comperator);

        uint dirtyEvicts = 0;
        for(uint i = 0; i< randSelector && !elements.empty(); i++){
            std::pop_heap(elements.begin(), elements.end(), comperator);
            ram_type::iterator element = elements.back();
            PID pid = element->first;
            elements.pop_back();
            handle_out_of_ram(pid);
            ram.erase(element);
            dirtyEvicts += postRemove(pid);
        }
        return dirtyEvicts;
    }

    static double
    eval_freq(ram_type::iterator& candidate, RefTime curr_time, bool write_as_read, uint write_cost, bool dirty) {
        double candidate_freq = get_frequency(candidate->second.first, curr_time);
        double candidate_freq_W = get_frequency(candidate->second.second, curr_time);
        if(dirty){
            candidate_freq += + candidate_freq_W * write_cost;
        }
        if(!write_as_read){
            candidate_freq += candidate_freq_W;
        }
        return candidate_freq;
    }


    static std::function<double(ram_type::iterator &, ram_type::iterator &)>
    gt_compare_freq(RefTime curr_time, bool write_as_read, uint write_cost, std::vector<bool>& dirty_in_ram) {
        return [curr_time, write_as_read, write_cost, &dirty_in_ram](ram_type::iterator& l, ram_type::iterator& r) {
            return eval_freq(l, curr_time, write_as_read, write_cost, dirty_in_ram[l->first]) > eval_freq(r, curr_time, write_as_read,
                                                                                             write_cost, dirty_in_ram[r->first]);
        };
    };


};

struct LFU_1K_E_real: public EvictStrategyKeepHistoryCombined{
    using upper = EvictStrategyKeepHistoryCombined;

    LFU_1K_E_real(uint K, uint randSize, uint randSelector = 1, uint epoch_size = 1, uint write_cost = 1) :
            upper(K, -1, epoch_size),
            randSelector(randSelector), // how many do we want to evict?
            randSize(randSize), // how many are evaluated
            writeCost(write_cost){}

    const uint randSelector, randSize, writeCost;

    uint rand_list_length;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        rand_list_length = calculate_rand_list_length(ram_size, randSize);
    }

    uint evict(Access access) override{
        RefTime curr_time = access.pos / epoch_size_iter;
        std::vector<ram_type::iterator> elements = getElementsFromRam<ram_type::iterator>(rand_list_length);

        // Sort elements by frequency; //std::min_element
        auto comperator = gt_compare_freq(curr_time, this->writeCost);
        std::make_heap(elements.begin(), elements.end(), comperator);

         uint dirtyEvicts = 0;
        for(uint i = 0; i< randSelector && !elements.empty(); i++){
            std::pop_heap(elements.begin(), elements.end(), comperator);
            ram_type::iterator element = elements.back();
            PID pid = element->first;
            elements.pop_back();
            handle_out_of_ram(pid);
            ram.erase(element);
            dirtyEvicts += postRemove(pid);
        }
        return dirtyEvicts;
    }

    static double
    eval_freq(const ram_type::iterator candidate, RefTime curr_time, uint write_cost = 1) {
        return get_frequency(candidate->second, curr_time, write_cost);
    }


    static std::function<double(const ram_type::iterator, const ram_type::iterator)>
    gt_compare_freq(RefTime curr_time, uint write_cost) {
        return [curr_time, write_cost](const ram_type::iterator l, const ram_type::iterator r) {
            return eval_freq(l, curr_time, write_cost) > eval_freq(r, curr_time, write_cost);
        };
    };


};

struct LFU_1K_E_real_vers2: public EvictStrategyKeepHistoryCombined{
    using upper = EvictStrategyKeepHistoryCombined;

    LFU_1K_E_real_vers2(uint K, uint randSize, uint randSelector = 1, uint epoch_size = 1, uint write_cost = 1) :
            upper(K, -1, epoch_size),
            randSelector(randSelector), // how many do we want to evict?
            randSize(randSize), // how many are evaluated
            writeCost(write_cost){}

    const uint randSelector, randSize, writeCost;

    uint rand_list_length;
    void reInit(RamSize ram_size) override{
        upper::reInit(ram_size);
        rand_list_length = calculate_rand_list_length(ram_size, randSize);
    }

    uint evict(Access access) override{
        RefTime curr_time = access.pos / epoch_size_iter;
        std::vector<ram_type::iterator> elements = getElementsFromRam<ram_type::iterator>(rand_list_length);

        // Sort elements by frequency; //std::min_element
        auto comperator = gt_compare_freq(curr_time, this->writeCost, dirty_in_ram);
        std::make_heap(elements.begin(), elements.end(), comperator);

        uint dirtyEvicts = 0;
        for(uint i = 0; i< randSelector && !elements.empty(); i++){
            std::pop_heap(elements.begin(), elements.end(), comperator);
            ram_type::iterator element = elements.back();
            PID pid = element->first;
            elements.pop_back();
            handle_out_of_ram(pid);
            ram.erase(element);
            dirtyEvicts += postRemove(pid);
        }
        return dirtyEvicts;
    }

    static double
    eval_freq(const ram_type::iterator candidate, RefTime curr_time, uint write_cost, bool is_write) {
        return get_frequency(candidate->second, curr_time, write_cost, is_write);
    }


    static std::function<double(const ram_type::iterator, const ram_type::iterator)>
    gt_compare_freq(RefTime curr_time, uint write_cost,
                    std::vector<bool>& map) {
        return [curr_time, write_cost, &map](const ram_type::iterator l, const ram_type::iterator r) {
            return eval_freq(l, curr_time, write_cost, map[l->first]) > eval_freq(r, curr_time, write_cost, map[r->first]);
        };
    };

    static double get_frequency(std::vector<std::pair<RefTime, bool>>& candidate, RefTime curr_time, [[maybe_unused]] uint write_cost, bool is_dirty){
        if(candidate.empty()){
            return 0;
        }
        long value = 1;
        long best_age = (curr_time - candidate[0].first) +1, best_value = value;
        for(uint pos = 1; pos < candidate.size(); pos++){
            value += 1;
            long age = (curr_time - candidate[pos].first) +1;
            long left = value*best_age;
            long right = best_value * age;
            if(left> right){
                best_value = value;
                best_age = age;
            }
        }
        if(is_dirty){
            best_value*=write_cost;
        }
        return best_value * 1.0 /best_age;
    }
};


struct LFUalt_K: public EvictStrategyContainer<std::unordered_set<PID>> {
    using upper = EvictStrategyContainer<std::unordered_set<PID>>;
    LFUalt_K(int K): upper(), K(K) {}
    uint K;

    std::unordered_map<PID, std::vector<RefTime>> history;
    void access(const Access& access) override{
        if(!in_ram[access.pid]){
            std::vector<RefTime>& list = history[access.pid];
            list.reserve(K);
        }
        push_frontAndResize(access, history[access.pid], K);
        ram.insert(access.pid);
    };
    PID evictOne(Access access) override{
        PID candidate = *ram.begin();
        double candidate_freq = get_frequency(history[candidate], access.pos);
        for(PID runner: ram){
            double runner_freq = get_frequency(history[runner], access.pos);
            if(runner_freq < candidate_freq){
                candidate = runner;
                candidate_freq = runner_freq;
            }
        }
        PID pid = candidate;
        ram.erase(candidate);
        history.erase(pid);
        return pid;
    }
};
