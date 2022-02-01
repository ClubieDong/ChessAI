#pragma once

#include <functional>
#include <utility>

namespace std {
template <typename T1, typename T2>
struct hash<std::pair<T1, T2>> {
    size_t operator()(const std::pair<T1, T2> &obj) const {
        return std::hash<T1>()(obj.first) ^ std::hash<T2>()(obj.second);
    }
};
} // namespace std
