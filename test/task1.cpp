#include <chrono>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <utility>
#include <stdexcept>
#include <cstdlib>
#include <random>
#include <sstream>
#include "/root/workspace/ART/ARTSynchronized/ART/Tree.h" 

constexpr size_t SIZE_200M = 200'000'000;
constexpr size_t SIZE_100M = 100'000'000;

std::vector<std::pair<uint64_t, uint64_t>> read_dataset(const std::string& filename, size_t required_size, bool random_values = false) {
    std::vector<std::pair<uint64_t, uint64_t>> data;
    std::ifstream file(filename, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    uint64_t total_count;
    file.read(reinterpret_cast<char*>(&total_count), sizeof(total_count));
    if (!file) {
        throw std::runtime_error("Failed to read data count from file: " + filename);
    }

    std::cout << "Total count in file: " << total_count << std::endl;

    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(sizeof(uint64_t), std::ios::beg);

    size_t actual_key_count = (file_size - sizeof(uint64_t)) / sizeof(uint64_t);
    
    if (actual_key_count != total_count) {
        std::cout << "Warning: Actual key count (" << actual_key_count 
                  << ") differs from reported count (" << total_count << ")." << std::endl;
    }

    data.reserve(required_size);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t key;
    uint64_t value = 0;
    size_t read_count = 0;
    while (data.size() < required_size) {
        if (read_count >= actual_key_count) {
            file.clear();
            file.seekg(sizeof(uint64_t), std::ios::beg);
            read_count = 0;
            std::cout << "Restarting file read. Current data size: " << data.size() << std::endl;
        }
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        if (!file) {
            break;
        }
        
        if (random_values) {
            value = dis(gen);
        } else {
            value++;
        }

        data.emplace_back(key, value);
        read_count++;
    }

    std::cout << "Read " << data.size() << " keys from file: " << filename << std::endl;

    return data;
}

template<typename Func>
std::pair<long long, double> time_operation(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    double avg_height = func();
    auto end = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return {duration, avg_height};
}

bool is_sorted(const std::vector<std::pair<uint64_t, uint64_t>>& data) {
    return std::is_sorted(data.begin(), data.end(), 
        [](const auto& a, const auto& b) { return a.first < b.first; });
}

std::pair<long long, double> test_art_insert(const std::vector<std::pair<uint64_t, uint64_t>>& data, size_t size) {
    return time_operation([&]() {
        ART_unsynchronized::Tree tree([](TID tid, Key& key) {
            key.set(reinterpret_cast<const char*>(&tid), sizeof(TID));
        });
        for (size_t i = 0; i < size; ++i) {
            Key key;
            key.set(reinterpret_cast<const char*>(&data[i].first), sizeof(uint64_t));
            tree.insert(key, data[i].second);
        }
        return tree.calculateAverageHeight();
    });
}

void run_tests(const std::string& dataset_name, std::vector<std::pair<uint64_t, uint64_t>>& original_data, std::ofstream& csv_file) {
    // std::vector<std::string> data_states = {"Original", "Sorted", "Shuffled"};
    std::vector<std::string> data_states = {"Original"};
    for (const auto& state : data_states) {
        std::vector<std::pair<uint64_t, uint64_t>> data = original_data;

        if (state == "Sorted") {
            std::sort(data.begin(), data.end());
        } else if (state == "Shuffled") {
            std::shuffle(data.begin(), data.end(), std::mt19937{std::random_device{}()});
        }

        bool is_data_sorted = is_sorted(data);
        
        auto [insert_time, avg_height] = test_art_insert(data, SIZE_200M);

        csv_file << dataset_name << "," << state << "," << (is_data_sorted ? "Yes" : "No") << ","
                 << insert_time << "," << avg_height << "\n";

        std::cout << "Dataset: " << dataset_name << ", State: " << state << std::endl;
        std::cout << "Is sorted: " << (is_data_sorted ? "Yes" : "No") << std::endl;
        std::cout << "Insert 200M time: " << insert_time << " ms" << std::endl;
        std::cout << "Average height: " << avg_height << std::endl;
        std::cout << std::endl;
    }
}

int main() {
    std::vector<std::string> datasets = {"osm", "fb", "covid", "planet"};
    std::ofstream csv_file("art_results.csv");
    csv_file << "Dataset,State,Is Sorted,Insert 200M (ms),Average Height\n";

    for (const auto& dataset : datasets) {
        std::cout << "Testing dataset: " << dataset << std::endl;
        
        try {
            auto data = read_dataset("../../datasets/" + dataset, SIZE_200M);
            run_tests(dataset, data, csv_file);
        } catch (const std::exception& e) {
            std::cerr << "Error processing dataset " << dataset << ": " << e.what() << std::endl;
            csv_file << dataset << ",Error," << e.what() << ",Error,Error\n";
        }
    }

    csv_file.close();
    std::cout << "Results have been written to art_results.csv" << std::endl;

    return 0;
}