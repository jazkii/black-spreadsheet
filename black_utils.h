#pragma once
#include <cstdint>
#include <functional>
#include <utility>

template <typename T>
bool ValidateBoundaries(T first, T last, T val) { // TODO: add const refs
  return val >= first && val < last;
}

namespace CombinedHashDetail {
  template <typename ...Hashes>
  std::size_t CombineHashes(std::size_t hash, Hashes ...hashes) {
    constexpr size_t kHashCoef = 402'653'189ull;
    return hash + kHashCoef * CombineHashes(hashes...);
  }
  template <>
  std::size_t CombineHashes<>(std::size_t hash);

  template <typename T>
  std::size_t ComputeHash(const T& obj) {
    return std::hash<T>{}(obj);
  }
}

template <typename ...Ts>
std::size_t ComputeCombinedHash(const Ts& ...ts) {
  return CombinedHashDetail::CombineHashes(CombinedHashDetail::ComputeHash(ts)...);
}
