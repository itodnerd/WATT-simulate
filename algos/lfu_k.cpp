//
// Created by dev on 27.10.21.
//

#include "lfu_k.hpp"

float minn(float a, float b) { return a<b ? a : b; }
float maxx(float a, float b) { return a>b ? a : b; }

using namespace std;

double get_frequency(std::vector<RefTime>& array, RefTime now, bool ignore_first) {
   float best = 0;
   uint i = 0;
   if(ignore_first) i++;
   for (; i<array.size(); i++)
      best = maxx(best, minn((float)(i+1) / (now-array[i]), 1.0));
   return best;
}

double get_frequency(std::vector<std::pair<RefTime, bool>>& candidate, RefTime curr_time, [[maybe_unused]] uint write_cost){
    if(candidate.empty()){
        return 0;
    }
    long value =candidate[0].second? write_cost*write_cost +1 : 1;
    long best_age = (curr_time - candidate[0].first) +1, best_value = value;
    for(uint pos = 1; pos < candidate.size(); pos++){
        value += candidate[pos].second? write_cost +1 : 1;
        long age = (curr_time - candidate[pos].first) +1;
        long left = value*best_age;
        long right = best_value * age;
        if(left> right){
            best_value = value;
            best_age = age;
        }
    }
    return best_value * 1.0 /best_age;
}
