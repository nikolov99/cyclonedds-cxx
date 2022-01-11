#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <optional>

#ifndef CYCLONE_WRAP_H_
#define CYCLONE_WRAP_H_

namespace org
{
namespace eclipse
{
namespace cyclonedds
{
namespace core
{

template <typename T>

struct OstreamWrap {
    const T& value;
    OstreamWrap(const T& value): value(value) {}
};

template <typename T>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<std::vector<T>> &x) {
    os << "vec(";
    for (const auto &it : x.value) {
        os << OstreamWrap{it} << ",";
    }
    return os << ")";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<std::optional<T>> &x) {
    os << "opt(";
    if (x.value) {
        os << OstreamWrap{x.value.value()};
    } else {
        os << "nullopt";
    }
    return os << ")";
}

template <typename T, std::size_t N>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<std::array<T, N>> &x) {
    os << "arr(";
    for (size_t i = 0; i < N; ++i) {
        os << (i == 0 ? "" : ", ") << OstreamWrap(x.value[i]);
    }
    return os << ")";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<T> &x) {
    return os << x.value;
}

}
}
}
}

#endif


