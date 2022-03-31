#include "common.h"
#include "black_utils.h"

#include <algorithm>
#include <cctype>
#include <tuple>

bool Position::operator == (const Position& rhs) const {
  return row == rhs.row && col == rhs.col;
}

bool Position::operator < (const Position& rhs) const {
  return std::tie(row, col) < std::tie(rhs.row, rhs.col);
}

bool Position::IsValid() const {
  return ValidateBoundaries(0, kMaxRows, row)
      && ValidateBoundaries(0, kMaxCols, col);
}

constexpr int kLettersCount = 'Z' - 'A' + 1;

std::string Position::ToString() const {
  std::string result;
  if (!IsValid()) return result;

  int current_col = col;

  bool not_first = false;
  do {
    current_col -= not_first; // edge case where only one letter
    not_first = true;
    result += static_cast<char>((current_col % kLettersCount) + 'A');
    current_col /= kLettersCount;
  } while (current_col);

  std::reverse(std::begin(result), std::end(result));
  result += std::to_string(row + 1);
  return result;
}

Position Position::FromString(std::string_view str) {
  auto is_upper = [] (char ch) -> bool { return std::isupper(ch); };
  auto is_digit = [] (char ch) -> bool { return std::isdigit(ch); };
  auto letters_end = std::find_if_not(std::begin(str), std::end(str), is_upper);
  auto digits_end = std::find_if_not(letters_end, std::end(str), is_digit);

  if (letters_end == std::begin(str)
   || letters_end == std::end(str)
   || letters_end == digits_end
   || digits_end != std::end(str))
  {
    return {-1, -1};
  }

  size_t row = 0, col = 0;
  bool not_first = false;
  for (auto letter_it = std::begin(str); letter_it != letters_end; ++letter_it) {
    col += not_first;
    not_first = true;

    col *= kLettersCount;
    col += *letter_it - 'A';

    if (!ValidateBoundaries<size_t>(0, kMaxCols, col)) {
      return {-1, -1};
    }
  }

  for (auto digit_it = letters_end; digit_it != digits_end; ++digit_it) {
    row *= 10;
    row += *digit_it - '0';
    if (!(ValidateBoundaries<size_t>(0, kMaxRows, row)
       || ValidateBoundaries<size_t>(0, kMaxRows, row - 1)))
    {
      return {-1, -1};
    }
  }

  return {static_cast<int>(row) - 1, static_cast<int>(col)};
}

bool Size::operator == (const Size& rhs) const {
  return rows == rhs.rows && cols == rhs.cols;
}

FormulaError::FormulaError(FormulaError::Category category)
  : category_(category)
  {
  }

FormulaError::Category FormulaError::GetCategory() const {
  return category_;
}

bool FormulaError::operator == (FormulaError rhs) const {
  return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
  switch (category_) {
  case Category::Ref:
    return "#REF!";
  case Category::Value:
    return "#VALUE!";
  case Category::Div0:
    return "#DIV/0!";
  }
  return ""; // make compiler happy
}

std::ostream& operator << (std::ostream& output, FormulaError fe) {
  return output << fe.ToString();
}
