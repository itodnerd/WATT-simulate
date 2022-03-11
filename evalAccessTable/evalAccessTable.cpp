#include <string>
#include <utility>
#include <fstream>
#include <filesystem>
#include "evalAccessTable.hpp"

using namespace std;

//
// Created by dev on 05.10.21.
//
static void getOrDefaultAndSet(map<int, int> &history, int new_value, int pageRef,
                               int default_value, int* value);

EvalAccessTable::EvalAccessTable(std::string  filename, std::string  out_dir, bool do_run): filename(std::move(filename)), output_dir(std::move(out_dir)){
        if(!do_run){
            return;
        }
        bool test = false;
        bool benchmark = true;
        runFromFilename(test, benchmark);
    };

void EvalAccessTable::init(bool ignore_last_run, int max_ram){
    getDataFile();
    if(ignore_last_run){
        max_ram = -1;
    }
    createLists(ignore_last_run, max_ram); // this runs "lru" (lru_stack_trace)
}
void EvalAccessTable::runFromFilename(bool test, bool benchmark) {
    // in case of full run (not test or benchmark)
    bool run_slow = false;
    bool full_run = false;
    // Ignore last if test run, else not!
    init(test, 10000);

    default_compare_algos();
    advanced_compare_algos();
    if(benchmark){
        advanced_with_variations_algos();

        for (int kr: {8, 4, 2, 0}) //{16, 8, 4, 2, 0})
            for (int kw: {8, 4, 2, 1, 0}) //{16,8,4,2,0})
            {
                if (kr == 0 && kw == 0)
                    continue;
                for (int e: {0, 20, 10, 5, 1})
                    for (int rsi: {1, 5, 10})
                        for (int rsa: {1, 5, 10})
                            for (int p: {0, 1})
                                for (int wc: {1, 2, 4, 8}) {
                                    if (kw == 0 && wc != 1)
                                        continue;
                                    string name = "lfu_vers2_kr" + to_string(kr)
                                                  + "_kw" + to_string(kw)
                                                  + "_e" + to_string(e)
                                                  + "_rsi" + to_string(rsi)
                                                  + "_rsa" + to_string(rsa)
                                                  + "_wc" + to_string(wc)
                                                  + "_p" + to_string(p);
                                    runAlgorithm(name, LFU_Generator(kr, kw, e, rsi, rsa, false, p, wc));
                                    runAlgorithm(name + "_war", LFU_Generator(kr, kw, e, rsi, rsa, true, p, wc));
                                }
            }
        for (int k: {8, 4, 2, 0}) //{16, 8, 4, 2, 0})
                for (int e: {0, 20, 10, 5, 1})
                    for (int rsi: {1, 5, 10})
                        for (int rsa: {1, 5, 10})
                            for (int wc: {0, 1, 2, 4, 8}) {
                                string name = "lfu_k" + to_string(k)
                                              + "_e" + to_string(e)
                                              + "_rsi" + to_string(rsi)
                                              + "_rsa" + to_string(rsa)
                                              + "_wc" + to_string(wc);
                                runAlgorithm(name, LFU_1K_E_real_Generator(k, e, rsi, rsa, wc));
                            }
        runAlgorithm("lfu_vers2_kr8_kw0_e20_rsi10_rsa1_wc1_p0_war", LFU_Generator(8, 0, 20, 10, 1, true, 0));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc1_p0", LFU_Generator(8, 4, 20, 10, 1, false, 0));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc2_p0", LFU_Generator(8, 4, 20, 10, 1, false, 0, 2));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc4_p0", LFU_Generator(8, 4, 20, 10, 1, false, 0, 4));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc8_p0", LFU_Generator(8, 4, 20, 10, 1, false, 0, 8));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc1_p0_war", LFU_Generator(8, 4, 20, 10, 1, true, 0));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc2_p0_war", LFU_Generator(8, 4, 20, 10, 1, true, 0, 2));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc4_p0_war", LFU_Generator(8, 4, 20, 10, 1, true, 0, 4));
        runAlgorithm("lfu_vers2_kr8_kw4_e20_rsi10_rsa1_wc8_p0_war", LFU_Generator(8, 4, 20, 10, 1, true, 0, 8));

    }
    if(!test && !benchmark) {
        advanced_with_variations_algos();
        if (full_run) {
            runAlgorithm("lru_alt", LRU_Generator());
            runAlgorithm("lru_alt1", LRU1_Generator());
            runAlgorithm("lru_alt2a", LRU2a_Generator());
            runAlgorithm("lru_alt2b", LRU2b_Generator());
            runAlgorithm("lru_alt3", LRU3_Generator());
            runAlgorithm("cf_lru10", CfLRUGenerator(10));
            runAlgorithm("cf_lru20", CfLRUGenerator(20));
            runAlgorithm("cf_lru70", CfLRUGenerator(70));
            runAlgorithm("cf_lru80", CfLRUGenerator(80));
            runAlgorithm("cf_lru90", CfLRUGenerator(90));
            runAlgorithm("lfu_k_alt01", LFUalt_K_Generator(1));
            runAlgorithm("lfu_k_alt02", LFUalt_K_Generator(2));
            runAlgorithm("lfu_k_alt10", LFUalt_K_Generator(10));
            runAlgorithm("lfu_k_alt20", LFUalt_K_Generator(20));
            runAlgorithm("lru_k_alt01", LRUalt_K_Generator(1));
            runAlgorithm("lru_k_alt02", LRUalt_K_Generator(2));
            runAlgorithm("lru_k_alt10", LRUalt_K_Generator(10));
            runAlgorithm("lru_k_alt20", LRUalt_K_Generator(20));
            // runAlgorithm("opt3", Opt3_Generator());// broken
            if (run_slow) {
                runAlgorithm("opt2", Opt2_Generator());
                runAlgorithm("lru_alt2", LRU2_Generator());
                runAlgorithm("cf_lru100", CfLRUGenerator(100));
            }
            for (int k: {16, 8, 4, 2}) {
                for (int z: {100, 10, 1, -1}) {
                    runAlgorithm("lfu_k" + std::to_string(k) + "_z" + std::to_string(z), LFU_K_Z_Generator(k, z));
                    // runAlgorithm("lfu2_k" + std::to_string(k) + "_z" + std::to_string(z), LFU2_K_Z_Generator(k, z));
                    runAlgorithm("lru_k" + std::to_string(k) + "_z" + std::to_string(z), LRU_K_Z_Generator(k, z));
                    runAlgorithm("lfu_2k" + std::to_string(k) + "_z" + std::to_string(z) + "_T",
                                 LFU_2K_Z_Generator(k, k, z, true));
                    runAlgorithm("lfu_2k" + std::to_string(k) + "_z" + std::to_string(z) + "_F",
                                 LFU_2K_Z_Generator(k, k, z, false));
                    runAlgorithm("lfu_2k" + std::to_string(k) + "_z" + std::to_string(z) + "_TR",
                                 LFU_2K_Z_rand_Generator(k, k, z, 5, true));
                    runAlgorithm("lfu_2k" + std::to_string(k) + "_z" + std::to_string(z) + "_FR",
                                 LFU_2K_Z_rand_Generator(k, k, z, 5, false));
                }
                runAlgorithm("lfu_k_real_F_e" + std::to_string(k), LFU_2K_E_real_Generator(8, 4, k, 5, 5, false, 0));
                runAlgorithm("lfu_k_real2_F_e" + std::to_string(k), LFU_2K_E_real_Generator(4, 8, k, 5, 5, false, 0));
                runAlgorithm("lfu_p1_k_real_F_e" + std::to_string(k), LFU_2K_E_real_Generator(8, 4, k, 5, 5, false, 1));
                runAlgorithm("lfu_p1_k_real2_F_e" + std::to_string(k),
                             LFU_2K_E_real_Generator(4, 8, k, 5, 5, false, 1));
                runAlgorithm("lfu_k_real_F_e" + std::to_string(k) + "_wc" + std::to_string(8),
                             LFU_Generator(8, 4, k, 5, 5, false, 0, 8));
                runAlgorithm("lfu_k_" + std::to_string(k), LFU_K_Generator(k));
            }
        }
    }

    printToFile();
}

void EvalAccessTable::advanced_with_variations_algos() {
    advanced_compare_algos();
    runAlgorithm("lean10", Lean_Generator(10));
    runAlgorithm("lean20", Lean_Generator(20));
    runAlgorithm("lean30", Lean_Generator(30));
    runAlgorithm("lean40", Lean_Generator(40));
    runAlgorithm("cf_lru30", CfLRUGenerator(30));
    runAlgorithm("cf_lru40", CfLRUGenerator(40));
    runAlgorithm("cf_lru50", CfLRUGenerator(50));
    runAlgorithm("cf_lru60", CfLRUGenerator(60));
    runAlgorithmNonParallel("StaticOpt2", StaticOpt(2));
    runAlgorithmNonParallel("StaticOpt4", StaticOpt(4));
    runAlgorithmNonParallel("StaticOpt8", StaticOpt(8));
}

void EvalAccessTable::advanced_compare_algos() {
    default_compare_algos();
    runAlgorithm("lean30", Lean_Generator(30));
    runAlgorithm("lru_wsr", LRU_WSR_Generator());
    runAlgorithm("cf_lru50", CfLRUGenerator(50));
}

void EvalAccessTable::default_compare_algos() {
    runAlgorithmNonParallel("StaticOpt", StaticOpt());
    runAlgorithm("opt", Opt_Generator());
    runAlgorithm("clock", CLOCK_Generator());
    runAlgorithm("random", Random_Generator());
    runAlgorithmNonParallel("StaticOpt", StaticOpt());
    runAlgorithm("lru_k2_z-1", LRU_K_Z_Generator(2, -1));
}

void EvalAccessTable::printToFile() {
    vector<string> names;

    ofstream out_stream;
    out_stream.open(output_file);
    // print algo names
    out_stream << first_csv_line <<endl;
    // print entries
    uint elements = data.size();
    for (auto &algo: read_write_list) {
        for(auto& entry: algo.second){
            out_stream <<
                algo.first << "," <<
                entry.first << "," <<
                elements << "," <<
                entry.second.first << "," <<
                entry.second.second << endl;
        }
    }
}

void EvalAccessTable::getDataFile() {
    {
        ifstream reader;
        reader.open(filename);
        assert(reader.good());
        string line;
        bool firstLine = true;
        map<int, int> last_access;
        int i = 0;
        while (getline(reader, line)) {
            if (firstLine) {
                firstLine = false;
                assert("pages,is_write" == line);
                continue;
            }
            Access &access = data.emplace_back();
            stringstream ss(line);
            string value;
            getline(ss, value, ',');
            access.pid = stoi(value);
            getline(ss, value);
            access.write = (value.find("rue") != std::string::npos);
            access.pos = i;

            getOrDefaultAndSet(last_access, i, access.pid, -1, &access.lastRef);

            i++;
        }
    }
    {
        map<int, int> next_access;
        int data_size = data.size();
        for (uint i = 0; i < data.size(); i++) {
            int pos = data_size - (i + 1);
            getOrDefaultAndSet(next_access, pos, data[pos].pid, data_size, &data[pos].nextRef);
        }
    }
}

void EvalAccessTable::createLists(bool ignore_last_run, int max_ram) {
    ifstream reader;
    reader.open(output_file);
    if (reader.good() && !ignore_last_run) {
        handleCsv(reader);
    } else {
        cout << "No old files found" << endl;
        filesystem::create_directory(output_dir);
    }
    runAlgorithmNonParallel("lru", LruStackDist(max_ram));
}

const rwListSubType& EvalAccessTable::getValues(std::string name) {
    assert(hasValues(name));
    return read_write_list[name];
}
bool EvalAccessTable::hasValues(std::string algo){
    return read_write_list.contains(algo);
}

bool EvalAccessTable::hasValue(std::string algo, RamSize ramSize){
     return hasValues(algo) &&
        read_write_list[algo].contains(ramSize);
}

bool EvalAccessTable::hasAllValues(std::string algo){
    if(ramSizes.size() == 0){
        return false;
    }
    for(RamSize size: ramSizes){
        if(!hasValue(algo, size)){
            return false;
        }
    }
    return true;
}

ramListType EvalAccessTable::missingValues(std::string algo){
    ramListType missing;
    for(RamSize size: ramSizes){
        if(!hasValue(algo, size)){
            missing.emplace(size);
        }
    }
    return missing;
}

void EvalAccessTable::handleCsv(ifstream &filestream){
    string line;
    bool first_line = true;
    while (getline(filestream, line)) {
        if (first_line) {
            assert(line == first_csv_line);
            first_line = false;
            continue;
        }
        stringstream ss(line);
        string algo, ram_size, elements, reads, writes;
        assert(getline(ss, algo, ','));
        assert(getline(ss, ram_size, ','));
        assert(getline(ss, elements, ','));
        assert(getline(ss, reads, ','));
        assert(getline(ss, writes, ','));
        assert((uint) stoi(elements) == data.size());
        read_write_list[algo][stoi(ram_size)] = std::make_pair(stoi(reads), stoi(writes));
        if(!ramSizes.contains(stoi(ram_size))){
            ramSizes.emplace(stoi(ram_size));
        }
    }
}


static void getOrDefaultAndSet(map<int, int> &history, int new_value, int pageRef,
                        int default_value, int* value) {
    auto element = history.find(pageRef);
    if (element == history.end()) {
        *value = default_value;
    } else {
        *value = element->second;
    }
    history[pageRef] = new_value;
}
