#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <optional>

#ifndef CYCLONE_WRAP_H_
#define CYCLONE_WRAP_H_

template <typename T>

struct Wrap {
    const T& value;
    Wrap(const T& value): value(value) {}
};

template <typename T>
std::ostream &operator<< (std::ostream &os, const Wrap<std::vector<T>> &x) {
    os << "vec(";
    for (const auto &it : x.value) {
        os << Wrap{it} << ",";
    }
    return os << ")";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const Wrap<std::optional<T>> &x) {
    os << "opt(";
    if (x.value) {
        os << Wrap{x.value.value()};
    } else {
        os << "nullopt";
    }
    return os << ")";
}

template <typename T, std::size_t N>
std::ostream &operator<< (std::ostream &os, const Wrap<std::array<T, N>> &x) {
    os << "arr(";
    for (size_t i = 0; i < N; ++i) {
        os << (i == 0 ? "" : ", ") << Wrap(x.value[i]);
    }
    return os << ")";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const Wrap<T> &x) {
    return os << x.value;
}

#endif


