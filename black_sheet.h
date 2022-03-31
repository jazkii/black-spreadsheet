#pragma once
#include "common.h"
#include "formula.h"
#include "black_cell.h"
#include "black_range.h"

#include <vector>
#include <ostream>

namespace Black {
class Sheet : public ISheet {
  std::vector<std::vector<std::unique_ptr<Black::Cell>>> table_;

  void ValidatePosition(Position pos) const;

  template <typename PrintFunc>
  void PrintImpl(std::ostream& output, PrintFunc&& printer) const;
  Black::Cell* GetCellImpl(Position pos) const;
  std::unique_ptr<Black::Cell>& GetCellRef(Position pos);

  void DeleteReferencesForCell(Position pos, const std::vector<Position>& refs);

  void ShrinkRow(std::vector<std::unique_ptr<Black::Cell>>& row);
  void ShrinkTable();

public:
  ICell* GetCell(Position pos) override;
  const ICell* GetCell(Position pos) const override;

  void SetCell(Position pos, std::string text) override;

  void ClearCell(Position pos) override;

  void InsertRows(int before, int count = 1) override;
  void InsertCols(int before, int count = 1) override;

  void DeleteRows(int first, int count = 1) override;
  void DeleteCols(int first, int count = 1) override;

  Size GetPrintableSize() const override;

  void PrintValues(std::ostream& output) const override;
  void PrintTexts(std::ostream& output) const override;
};

template <typename PrintFunc>
void Black::Sheet::PrintImpl(std::ostream& output, PrintFunc&& printer) const {
  const auto size = GetPrintableSize();

  for (auto& row : Head(table_, size.rows)) {
    int col_index = 0;

    for (auto& cell_holder : Head(row, size.cols)) {
      if (cell_holder) {
        printer(output, cell_holder);
      }

      if (col_index + 1 != size.cols) {
        output << '\t';
      }

      ++col_index;
    }

    for (; col_index + 1 < size.cols; ++col_index) { // before \n \t is not printed
      output << '\t';
    }
    output << '\n';
  }
}
} // namespace Black