//
// Created by dev on 06.10.21.
//

#include <filesystem>
#include <cassert>
#include <iostream>
#include "lruStackDist.hpp"

void LruStackDist::evaluateRamList(const std::vector<Access> &data, ramListType &ramList,
                     rwListSubType &readWriteList){
    assert(ramList.size() == 0 && readWriteList.size() == 0);
    std::vector<int> lruStack, lruStackDist, lru_stack_dirty, dirty_depth;

    for (auto &access: data) {
        // read
        // find pos;
        auto elem = std::find(lruStack.begin(), lruStack.end(), access.pid);
        uInt pos = 0;
        if (elem != lruStack.end()) {
            pos = elem - lruStack.begin() + 1;
            lruStack.erase(elem);
        }
        // put to pos 0;
        lruStack.emplace(lruStack.begin(), access.pid);
        if (lruStackDist.size() <= pos) {
            lruStackDist.resize(pos + 1, 0);
            lru_stack_dirty.resize(pos + 1, 0);
        }
        lruStackDist[pos]++;
        // write
        // Idea:
        // Every written page has to be written out once.
        // it has only to be written out, if it moved down the stack to mutch in between
        // so if maxdepth between two writes is to big, it adds one write
        // se we have an extra counter, to mark, where the page went down in stack since last write.
        // when write happens, we mark the last position
        // in the End: we have to write all out => add a constant
        if (dirty_depth.size() <= access.pid) {
            dirty_depth.resize(access.pid + 1, 0); // clean
        }
        uInt prev_dirty_depth = dirty_depth[access.pid];
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
        int ram_size = 20;
        if (pages > 1000) {
            ram_size = 100;
        }
        if (pages > 10000) {
            ram_size = 1000;
        }
        ramList.emplace(ram_size);

        do {
            if (ram_size < 100) {
                ram_size += 10;
            } else if (ram_size < 1000) {
                ram_size += 100;
            } else if (ram_size < 10000) {
                ram_size += 1000;
            } else if (ram_size < 100000) {
                ram_size += 10000;
            } else {
                ram_size += 100000;
            }
            ramList.emplace(ram_size);

        } while (ram_size < pages);
    }// sum it up, buttercup!
    for (auto &ram_size: ramList) {
        int misses = 0, evicts = 0;
        for (uInt i = 0; i < lruStackDist.size(); i++) {
            if (i > ram_size || i== 0) {
                misses += lruStackDist[i];
            }
        }
        for (uInt i = 0; i < lru_stack_dirty.size(); i++) {
            if (i > ram_size || i == 0) {
                evicts += lru_stack_dirty[i];
            }
        }
        readWriteList[ram_size] = std::make_pair(misses, evicts);
    }
}