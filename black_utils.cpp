#include "black_utils.h"

namespace CombinedHashDetail {
  template <>
  std::size_t CombineHashes<>(std::size_t hash) {
    return hash;
  }
}