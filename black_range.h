#pragma once
#include <iterator>
#include <algorithm>

template <typename Iterator>
class Range {
  Iterator first_, last_;
public:
  explicit Range(Iterator first, Iterator last)
   : first_(first)
   , last_(last)
  {
  }

  Iterator begin() const {
    return first_;
  }
  Iterator end() const {
    return last_;
  }
  size_t size() const {
    return last_ - first_;
  }
};

template <typename Container>
auto Head(Container& c, size_t count) {
  return Range{std::begin(c), std::next(std::begin(c), std::min(c.size(), count))};
}