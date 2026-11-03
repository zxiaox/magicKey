#pragma once
#include <algorithm>
#include <vector>
#include <stdexcept>
namespace mywarkey {
struct Combination { std::vector<int> keys; int target = 0; };
inline std::vector<int> combinationKeys(std::vector<int> keys) {
    if (keys.size() < 2 || keys.size() > 3) throw std::runtime_error("A combination needs two or three keys.");
    std::sort(keys.begin(), keys.end());
    if (keys.front() < 1 || keys.back() > 254 || std::adjacent_find(keys.begin(), keys.end()) != keys.end())
        throw std::runtime_error("Combination keys must be distinct valid keys.");
    return keys;
}
}
