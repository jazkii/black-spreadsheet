#pragma once

#include "common.h"
#include "black_utils.h"

namespace Black {
  struct PositionHash {
    std::size_t operator () (const Position& pos) const {
      return ComputeCombinedHash(pos.row, pos.col);
    }
  };
} // namespace Black