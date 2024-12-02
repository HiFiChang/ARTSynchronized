#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include "/root/workspace/ART/ARTSynchronized/ART/Tree.h"

// 生成测试数据
std::vector<std::pair<uint64_t, uint64_t>> generate_test_data(size_t size) {
    std::vector<std::pair<uint64_t, uint64_t>> data;
    data.reserve(size);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    // for (size_t i = 5; i < size; ++i) {
    //     // data.emplace_back(dis(gen), i);
    //     data.emplace_back(i, i+1);
    //     // data.emplace_back(i, dis(gen));
    //     // data.emplace_back(i, 10);
    //     uint64_t key = dis(gen);
    //     // data.emplace_back(key, key);
    // }
    data.emplace_back(1, 1);
    data.emplace_back(2, 1);

    return data;
}

// 验证树的正确性
bool verify_tree(const ART_unsynchronized::Tree& tree, 
                const std::vector<std::pair<uint64_t, uint64_t>>& data,
                const std::string& operation_name) {
    bool all_found = true;
    int count = 0;
    for (const auto& [key_val, value] : data) {
        Key search_key;
        search_key.set(reinterpret_cast<const char*>(&key_val), sizeof(uint64_t));
        TID result = tree.lookup(search_key);
        count++;
        if (result != value) {
            all_found = false;
            std::cout << "Error in " << operation_name << ": Key " << key_val 
                     << " not found or has incorrect value." << std::endl;
            std::cout << "Expected: " << value << ", Found: " << result << std::endl;
            std::cout << "Count: " << count << std::endl;
            break;
        }
    }
    return all_found;
}

// 测试bulkload功能
void test_bulkload(const std::vector<std::pair<uint64_t, uint64_t>>& data) {
    std::cout << "\n=== Testing Bulkload ===" << std::endl;
    
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

    // 执行bulkload并计时
    auto start = std::chrono::high_resolution_clock::now();
    tree.bulkload(bulkload_data);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 验证结果
    bool success = verify_tree(tree, data, "bulkload");

    // 输出结果
    std::cout << "Bulkload completed in " << duration << " ms." << std::endl;
    std::cout << "Tree average height: " << tree.calculateAverageHeight() << std::endl;
    std::cout << "All keys found and correct: " << (success ? "Yes" : "No") << std::endl;
}

// 测试insert功能
void test_insert(const std::vector<std::pair<uint64_t, uint64_t>>& data) {
    std::cout << "\n=== Testing Insert ===" << std::endl;
    
    // 初始化树结构
    ART_unsynchronized::Tree tree([](TID tid, Key& key) {
        key.set(reinterpret_cast<const char*>(&tid), sizeof(TID));
    });

    // 执行插入操作并计时
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& [key_val, value] : data) {
        Key insert_key;
        insert_key.set(reinterpret_cast<const char*>(&key_val), sizeof(uint64_t));
        tree.insert(insert_key, value);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 验证结果
    bool success = verify_tree(tree, data, "insert");

    // 输出结果
    std::cout << "Insert completed in " << duration << " ms." << std::endl;
    std::cout << "Tree average height: " << tree.calculateAverageHeight() << std::endl;
    std::cout << "All keys found and correct: " << (success ? "Yes" : "No") << std::endl;
}

int main() {
    const size_t TEST_SIZE = 1000;
    auto test_data = generate_test_data(TEST_SIZE);
    
    std::cout << "Testing with " << TEST_SIZE << " elements..." << std::endl;
    
    // 测试 bulkload
    test_bulkload(test_data);
    
    // 测试 insert
    test_insert(test_data);
    
    return 0;
}
