#ifndef CYCLONEDDS_CORE_OSTREAM_WRAP_HPP_
#define CYCLONEDDS_CORE_OSTREAM_WRAP_HPP_

#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <optional>

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
    os << "{";
    for (const auto &it : x.value) {
        os << OstreamWrap{it} << (x.value.back()==it ? "" : ", ");
    }
    return os << "}";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<std::optional<T>> &x) {
    if (x.value) {
        os << OstreamWrap{x.value.value()};
    } else {
        os << "null";
    }    
    return os;
}

template <typename T, std::size_t N>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<std::array<T, N>> &x) {
    os << "{";
    for (size_t i = 0; i < N; ++i) {
        os << OstreamWrap(x.value[i]) << (i < (N-1) ? ", " : "");
    }
    return os << "}";
}

template <typename T>
std::ostream &operator<< (std::ostream &os, const OstreamWrap<T> &x) {
    return os << x.value;
}

}
}
}
}

#endif /* CYCLONEDDS_CORE_OSTREAM_WRAP_HPP_ */