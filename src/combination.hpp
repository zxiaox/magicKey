#pragma once
#include <algorithm>
#include <vector>
#include <stdexcept>
namespace mywarkey {
enum class CombinationMode { InputChord = 0, Together = 1, Sequence = 2 };
struct Combination { std::vector<int> keys; int target = 0; CombinationMode mode = CombinationMode::InputChord; std::vector<int> fields; };
inline std::vector<int> combinationKeys(std::vector<int> keys, CombinationMode mode = CombinationMode::InputChord) {
    if (keys.size() < 2 || keys.size() > 5) throw std::runtime_error("A combination needs two to five keys.");
    for (int key : keys) if (key < 1 || key > 254) throw std::runtime_error("Invalid combination key.");
    if (mode == CombinationMode::Sequence) return keys;
    auto original = keys;
    std::sort(keys.begin(), keys.end());
    if (keys.front() < 1 || keys.back() > 254 || std::adjacent_find(keys.begin(), keys.end()) != keys.end())
        throw std::runtime_error("Combination keys must be distinct valid keys.");
    return mode == CombinationMode::Together ? original : keys;
}
}
