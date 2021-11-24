//
// Created by dev on 06.10.21.
//
#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#ifndef C_GENERAL_HPP
#define C_GENERAL_HPP

#endif //C_GENERAL_HPP
using uInt = unsigned int;
using PID = uInt;
using RefTime = int;
using RamSize = uInt;
using ramListType = std::unordered_set<RamSize>;
using rwListSubType = std::unordered_map<RamSize, std::pair<uInt, uInt>>;
using rwListType = std::unordered_map<std::string, rwListSubType>;

struct Access {
    PID pid=0;
    RefTime nextRef=0;
    RefTime lastRef=-1;
    RefTime pos=0;
    bool write=false;
};

