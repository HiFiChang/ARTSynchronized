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

bool is_sorted(const std::vector<std::pair<uint64_t, uint64_t>>& data) {
    return std::is_sorted(data.begin(), data.end(), 
        [](const auto& a, const auto& b) { return a.first < b.first; });
}

std::vector<std::pair<uint64_t, uint64_t>> read_dataset(const std::string& filename, size_t required_size, bool random_values = false) {
    std::vector<std::pair<uint64_t, uint64_t>> data;
    std::ifstream file(filename, std::ios::binary);
    
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    // 读取第一个uint64_t作为数据量
    uint64_t total_count;
    file.read(reinterpret_cast<char*>(&total_count), sizeof(total_count));
    if (!file) {
        throw std::runtime_error("Failed to read data count from file: " + filename);
    }

    std::cout << "Total count in file: " << total_count << std::endl;

    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(sizeof(uint64_t), std::ios::beg);  // 跳过第一个计数值

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
            value = dis(gen);  // 生成随机值
        } else {
            value++;  // 按顺序生成值
        }
        
        // data.emplace_back(key, value);
        data.emplace_back(key, key);
        read_count++;
    }

    std::cout << "Read " << data.size() << " keys from file: " << filename << std::endl;

    return data;
}

// 测试bulkload功能
void test_bulkload(const std::vector<std::pair<uint64_t, uint64_t>>& data) {
    // 初始化树结构
    ART_unsynchronized::Tree tree([](TID tid, Key& key) {
        key.set(reinterpret_cast<const char*>(&tid), sizeof(TID));
    });

    // 准备用于 bulkload 的数据
    std::vector<std::pair<Key, TID>> bulkload_data;
    bulkload_data.reserve(data.size());

    // 将 uint64_t 的 key 和 value 转换为 Key 和 TID 类型
    for (const auto& [key_val, value] : data) {
        Key key;
        key.set(reinterpret_cast<const char*>(&key_val), sizeof(uint64_t));
        bulkload_data.emplace_back(std::move(key), value);
        // std::cout << "key: " << key_val << " value: " << value << std::endl;
    }

    // 对 bulkload_data 进行排序
    std::sort(bulkload_data.begin(), bulkload_data.end(),
        [](const auto& a, const auto& b) {
            return std::lexicographical_compare(
                a.first.getData(), a.first.getData() + a.first.getKeyLen(),
                b.first.getData(), b.first.getData() + b.first.getKeyLen()
            );
        }
    );

    // 执行bulkload
    auto start = std::chrono::high_resolution_clock::now();
    
    // 调用新的 bulkLoad 函数
    // tree.bulkLoad(bulkload_data, tree.getRoot(), 0); // 传入根节点和初始层级
    tree.bulkload(bulkload_data);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 验证bulkload结果
    bool all_found = true;
    int count = 0;
    for (const auto& [key_val, value] : data) {
        Key search_key;
        search_key.set(reinterpret_cast<const char*>(&key_val), sizeof(uint64_t));
        TID result = tree.lookup(search_key);
        count++;
        if (result != value) {
            all_found = false;
            std::cout << "Error: Key " << key_val << " not found or has incorrect value." << std::endl;
            std::cout << "Expected: " << value << ", Found: " << result << std::endl;
            std::cout << "Count: " << count << std::endl;
            break;
        }
    }

    // 输出结果
    std::cout << "Bulkload completed in " << duration << " ms." << std::endl;
    std::cout << "Tree average height: " << tree.calculateAverageHeight() << std::endl;
    std::cout << "All keys found and correct: " << (all_found ? "Yes" : "No") << std::endl;
}

void run_tests(const std::string& dataset_name, std::vector<std::pair<uint64_t, uint64_t>>& data, std::ofstream& csv_file) {
    std::vector<std::string> data_states = {"Original", "Sorted", "Shuffled"};
    
    for (const auto& state : data_states) {
        std::vector<std::pair<uint64_t, uint64_t>> test_data = data;
        // 临时设置123为测试数据
        // test_data = {{1, 2}};

        if (state == "Sorted") {
            std::sort(test_data.begin(), test_data.end());
        } else if (state == "Shuffled") {
            std::shuffle(test_data.begin(), test_data.end(), std::mt19937{std::random_device{}()});
        }

        bool is_data_sorted = is_sorted(test_data);

        // 运行 bulkLoad 测试
        auto start = std::chrono::high_resolution_clock::now();
        test_bulkload(test_data);  // 执行 bulkload 测试
        auto end = std::chrono::high_resolution_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // 记录结果到 CSV 文件
        csv_file << dataset_name << "," << state << "," << (is_data_sorted ? "Yes" : "No") << "," << duration << ",N/A\n";
    }
}

int main() {
    std::vector<std::string> datasets = {"osm"};
    std::ofstream csv_file("bulkload.csv");
    csv_file << "Dataset,State,Is Sorted,Bulkload Time (ms),Average Height\n";

    for (const auto& dataset : datasets) {
        std::cout << "Testing dataset: " << dataset << std::endl;
        
        try {
            // 从文件读取数据集
            auto data = read_dataset("../../datasets/" + dataset, 10000);
            run_tests(dataset, data, csv_file);
        } catch (const std::exception& e) {
            std::cerr << "Error processing dataset " << dataset << ": " << e.what() << std::endl;
            csv_file << dataset << ",Error,Error,Error,Error\n";
        }
    }

    csv_file.close();
    std::cout << "Results have been written to art_bulkload_results.csv" << std::endl;

    return 0;
}