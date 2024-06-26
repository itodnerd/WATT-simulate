//
// Created by dev on 06.10.21.
//

#include <filesystem>
#include <cassert>
#include <iostream>
#include <list>
#include "lruStackDist.hpp"
#include <algorithm>

void LruStackDist::evaluateRamList(const std::vector<Access> &data, ramListType &ramList,
                     rwListSubType &readWriteList){
    assert(ramList.size() == 0 && readWriteList.size() == 0);
    std::list<PID> lruStack;
    std::vector<int> lruStackDist, lru_stack_dirty;
    std::unordered_map<PID, typename std::list<PID>::iterator> fast_finder;
    std::unordered_map<PID, int> dirty_depth;
    for (auto &access: data) {
        // read
        // find pos;
        uint pos = 0;
        if(fast_finder.find(access.pid)!= fast_finder.end()){
            pos = 1;
            std::list<PID>::iterator pointer = lruStack.begin();
            while(*pointer != access.pid){
                pos ++;
                pointer ++;
            }
            lruStack.erase(pointer);
        }

        // put to pos 0;
        lruStack.emplace_front(access.pid);
        if (lruStackDist.size() <= pos) {
            lruStackDist.resize(pos + 1, 0);
            lru_stack_dirty.resize(pos + 1, 0);
        }
        fast_finder[access.pid] = lruStack.begin();
        lruStackDist[pos]++;
        // write
        // Idea:
        // Every written page has to be written out once.
        // it has only to be written out, if it moved down the stack to much in between
        // so if maxdepth between two writes is to big, it adds one write
        // se we have an extra counter, to mark, where the page went down in stack since last write.
        // when write happens, we mark the last position
        // in the End: we have to write all out => add a constant
        if(dirty_depth.find(access.pid) == dirty_depth.end()){
            dirty_depth[access.pid] = 0;
        }
        uint prev_dirty_depth = dirty_depth[access.pid];
        // is fresh write? => init
        // has no prev dirty_depth: keep 0;
        // else: move down in stack;
        dirty_depth[access.pid] = access.write ? 1 : (prev_dirty_depth == 0 ? 0 : std::max(pos, prev_dirty_depth));
        if (access.write) {
            if (prev_dirty_depth != 0) {
                lru_stack_dirty[std::max(prev_dirty_depth, pos)]++;
            } else {
                lru_stack_dirty[0]++; // increase constant;
            }
        }
    }

    {
        int pages = lruStackDist[0];
        std::vector<RamSize> list;
        int ram_size = 100;
        do {
            list.push_back(ram_size);
            if (pages < 3000 || ram_size < 1000) {
                ram_size += 100;
            } else if(pages < 30000 || ram_size < 10000) {
                ram_size += 1000;
            } else if(pages < 300000 || ram_size < 100000) {
                ram_size += 10000;
            } else {
                ram_size += 100000;
            }
        } while (ram_size < pages && (ram_size < max_ram || max_ram == -1));
        list.push_back(ram_size);
        std::reverse(list.begin(), list.end());
        for(uint i =0; i< 20 && i < list.size(); i++){
            ramList.emplace(list[i]);
        }
        ramList.emplace(100);
    }// sum it up, buttercup!
    for (auto &ram_size: ramList) {
        int misses = 0, evicts = 0;
        for (uint i = 0; i < lruStackDist.size(); i++) {
            if (i > ram_size || i== 0) {
                misses += lruStackDist[i];
            }
        }
        for (uint i = 0; i < lru_stack_dirty.size(); i++) {
            if (i > ram_size || i == 0) {
                evicts += lru_stack_dirty[i];
            }
        }
        readWriteList[ram_size] = std::make_pair(misses, evicts);
    }
}