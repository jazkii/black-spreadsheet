#include "black_sheet.h"
#include "black_utils.h"

#include <algorithm>

namespace Black {
  void Sheet::ValidatePosition(Position pos) const {
    if (!pos.IsValid()) {
      throw InvalidPositionException("");
    }
  }

  Size Sheet::GetPrintableSize() const {
    Size size{};

    int rows = 0;
    for (auto& row : table_) {
      ++rows;
      int cols = 0;
      for (auto& cell : row) {
        ++cols;
        if (cell && !cell->Empty()) {
          size.rows = rows;
          size.cols = std::max(size.cols, cols);
        }
      }
    }
    return size;
  }
void Sheet::PrintValues(std::ostream& output) const {
    PrintImpl(output,
      [] (std::ostream& output, const auto& cell_holder) {
        std::visit([&] (const auto& value) { output << value; }, cell_holder->GetValue());
      }
    );
  }

  void Sheet::PrintTexts(std::ostream& output) const {
    PrintImpl(output,
      [] (std::ostream& output, const auto& cell_holder) {
        output << cell_holder->GetText();
      }
    );
  }

  Black::Cell* Sheet::GetCellImpl(Position pos) const {
    ValidatePosition(pos);

    if (size_t(pos.row) < table_.size()) {
      auto& row = table_[pos.row];
      if (size_t(pos.col) < row.size()) {
        return row[pos.col].get();
      }
    }
    return nullptr;
  }

  std::unique_ptr<Black::Cell>& Sheet::GetCellRef(Position pos) {
    table_.resize(std::max<size_t>(table_.size(), pos.row + 1));
    auto& row  = table_[pos.row];
    row.resize(std::max<size_t>(row.size(), pos.col + 1));
    return row[pos.col];
  }

  const ICell* Sheet::GetCell(Position pos) const {
    return GetCellImpl(pos);
  }

  ICell* Sheet::GetCell(Position pos) {
    return GetCellImpl(pos);
  }

  void Sheet::ShrinkRow(std::vector<std::unique_ptr<Cell>>& row) {
    while (!row.empty() && !row.back()) {
      row.pop_back();
    }
  }

  void Sheet::ShrinkTable() {
    while (!table_.empty() && table_.back().empty()) {
      table_.pop_back();
    }
  }

  void Sheet::DeleteReferencesForCell(Position pos, const std::vector<Position>& refs) {
    for (auto referenced_cell_pos : refs) {
      auto& row = table_[referenced_cell_pos.row];
      auto& cell = row[referenced_cell_pos.col];

      cell->RemoveIncomingRef(pos);
      if (cell->Empty() && !cell->HasIncomingRefs()) {
        cell = nullptr;

        ShrinkRow(row);
      }
    }
    ShrinkTable();
  }

  void Sheet::ClearCell(Position pos) {
    auto* cell = GetCellImpl(pos);
    if (!cell) {
      return;
    }

    DeleteReferencesForCell(pos, cell->GetReferencedCells());

    if (cell->HasIncomingRefs()) {
      cell->Clear();
      return;
    }

    auto& row = table_[pos.row];
    row[pos.col] = nullptr;

    ShrinkRow(row);
    ShrinkTable();
  }

  void Sheet::SetCell(Position pos, std::string text) {
    ValidatePosition(pos);

    auto& cell = GetCellRef(pos);

    if (cell && cell->GetText() == text) {
      return;
    }

    auto new_cell_holder = std::make_unique<Black::Cell>(
      *this, pos, std::move(text), cell && cell->HasIncomingRefs()
    );
    if (cell) {
      cell->InvalidateCache();
      DeleteReferencesForCell(pos, cell->GetReferencedCells());
      new_cell_holder->SetIncomingReferences(cell->ReleaseIncomingReferences());
    }
    cell = std::move(new_cell_holder);

    for (auto referenced_cell_pos : cell->GetReferencedCells()) {
      auto& referenced_cell = GetCellRef(referenced_cell_pos);
      if (!referenced_cell) {
        referenced_cell = std::make_unique<Black::Cell>(*this, referenced_cell_pos, "", false);
      }
      referenced_cell->AddIncomingRef(pos);
    }
  }

  void Sheet::InsertRows(int before, int count) {
    bool insert_in_the_middle = table_.size() > size_t(before);

    int new_rows = (table_.size() + count) * insert_in_the_middle + (before + count) * !insert_in_the_middle;
    if (new_rows > Position::kMaxRows) {
      throw TableTooBigException("");
    }

    if (insert_in_the_middle && count) {
       for (auto& row : table_) {
         for(auto& cell : row) {
           if (cell) {
             cell->HandleInsertedRows(before, count);
           }
         }
       }

      table_.resize(table_.size() + count);
      std::rotate(std::begin(table_) + before, std::end(table_) - count, std::end(table_));
    }
  }

  void Sheet::InsertCols(int before, int count) {
    auto max_cols_it = std::max_element(
      std::begin(table_), std::end(table_),
      [] (const auto& lhs, const auto& rhs) { return lhs.size() < rhs.size(); }
    );
    const int max_cols = max_cols_it == std::end(table_) ? 0 : max_cols_it->size();
    bool insert_in_the_middle = max_cols > before;

    int new_cols = (max_cols + count) * insert_in_the_middle + (before + count) * !insert_in_the_middle;
    if (new_cols > Position::kMaxCols) {
      throw TableTooBigException("");
    }

    if (insert_in_the_middle && count) {
      for (auto& row : table_) {
        for (auto& cell : row) {
          if(cell) {
            cell->HandleInsertedCols(before, count);
          }
        }
        if (row.size() > size_t(before)) {
          row.resize(row.size() + count);
          std::rotate(std::begin(row) + before, std::end(row) - count, std::end(row));
        }
      }
    }
  }

  void Sheet::DeleteRows(int first, int count) {
    bool erase_in_the_middle = table_.size() > size_t(first);
    if (erase_in_the_middle && count) {
      table_.erase(
        std::begin(table_) + first,
        std::begin(table_) + first + std::min<size_t>(table_.size() - first, count)
      );
      for (auto& row : table_) {
        for(auto& cell : row) {
          if (cell) {
            cell->HandleDeletedRows(first, count);
            if (cell->Empty() && !cell->HasIncomingRefs()) {
              cell = nullptr;
            }
          }
        }
        ShrinkRow(row);
      }
      ShrinkTable();
    }
  }

  void Sheet::DeleteCols(int first, int count) {
    if (!count) return;

    bool erase_in_the_middle = false;
    for (auto& row : table_) {
      auto erase_begin_it = std::begin(row) + std::min<size_t>(row.size(), first);

      erase_in_the_middle = erase_begin_it != std::end(row);

      row.erase(
        erase_begin_it,
        erase_begin_it + std::min<size_t>(std::end(row) - erase_begin_it, count)
      );
    }
    if (erase_in_the_middle) {
      for (auto& row : table_) {
        for(auto& cell : row) {
          if (cell) {
            cell->HandleDeletedCols(first, count);
            if (cell->Empty() && !cell->HasIncomingRefs()) {
              cell = nullptr;
            }
          }
        }
        ShrinkRow(row);
      }
      ShrinkTable();
    }
  }

} // namespace Black

std::unique_ptr<ISheet> CreateSheet() {
  return std::make_unique<Black::Sheet>();
}
