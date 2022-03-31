#include "black_cell.h"
#include "black_utils.h"

#include <algorithm>
#include <cassert>

namespace Black {
  Cell::Cell(ISheet& sheet, Position pos, std::string text, bool has_incoming_refs)
    : sheet_(sheet)
  {
    if (text.empty() || text.front() == kEscapeSign || text.front() != kFormulaSign) {
      data_ = std::move(text);
    } else {
      data_ =  ParseFormula(text.substr(1));

      if (CheckForCircularDependency(pos, has_incoming_refs)) {
        throw CircularDependencyException(
          pos.ToString() + "=" + text
        );
      }
    }
  }

  ICell::Value Cell::GetValue() const {
    if (!value_cache_) {
      if (data_.IsText()) {
        const auto& text = data_.GetText();
        value_cache_ = text.substr(!text.empty() && text.front() == kEscapeSign); // remove escape sign
      } else {
        std::visit([&] (auto val) { value_cache_ = val; },
                   data_.GetFormula()->Evaluate(sheet_));
      }
    }
    return *value_cache_;
  }

  std::string Cell::GetText() const {
    return data_.IsFormula()
            ?  "=" + data_.GetFormula()->GetExpression()
            : data_.GetText();
  }

  std::vector<Position> Cell::GetReferencedCells() const {
    return data_.IsFormula()
           ? data_.GetFormula()->GetReferencedCells()
           : std::vector<Position>{};
  }

  void Cell::InvalidateCache() {
    if (!value_cache_) {
      return;
    }

    if (data_.IsFormula()) {
      data_.GetFormula()->InvalidateCache();
    }
    value_cache_ = std::nullopt;
    for (Position incoming_ref_pos : incoming_refs_) {
      auto* cell = dynamic_cast<Black::Cell*>(sheet_.GetCell(incoming_ref_pos));
      assert(cell); // only Black::Cell* supported
      cell->InvalidateCache();
    }

  }

  bool Cell::HasIncomingRefs() const {
    return !incoming_refs_.empty();
  }

  void Cell::AddIncomingRef(Position pos) {
    auto pos_it = std::lower_bound(
      std::begin(incoming_refs_), std::end(incoming_refs_), pos
    );
    if(pos_it == std::end(incoming_refs_) || !(pos == *pos_it)) {
      incoming_refs_.insert(pos_it, pos);
    }
  }

  void Cell::SetIncomingReferences(std::vector<Position> refs) {
    incoming_refs_ = std::move(refs);
  }

  std::vector<Position> Cell::ReleaseIncomingReferences() {
    return std::move(incoming_refs_); // incoming_refs_ is in unspecified state now, but it is
  }                                   // expected that this cell is going to be destroyed soon

  void Cell::RemoveIncomingRef(Position pos) {
    auto pos_it = std::lower_bound(std::begin(incoming_refs_), std::end(incoming_refs_), pos);
    if (pos_it != std::end(incoming_refs_))
      incoming_refs_.erase(pos_it);
  }

  bool Cell::Empty() const {
    return data_.IsText() && data_.GetText().empty();
  }

  void Cell::Clear() {
    if (!Empty()){
      data_ = "";
      InvalidateCache();
    }
  }

  bool Cell::CheckForCircularDependency(Position pos, bool has_incoming_refs) const {
    if (has_incoming_refs) {
      std::unordered_set<Position, Black::PositionHash> checked;
      return CheckForCircularDependencyImpl(pos, checked);
    }

    const auto referenced_cells = GetReferencedCells();
    return std::binary_search(std::begin(referenced_cells), std::end(referenced_cells), pos);
  }
  bool Cell::CheckForCircularDependencyImpl(Position pos,
                                            std::unordered_set<Position, Black::PositionHash>& checked) const
  {
    if (data_.IsText()) {
      return false;
    }
    const auto referenced_cells = GetReferencedCells();
    if (std::binary_search(std::begin(referenced_cells), std::end(referenced_cells), pos)) {
      return true;
    }
    return std::any_of(std::begin(referenced_cells), std::end(referenced_cells),
                       [&, pos] (Position cell_pos) {
                         if (checked.count(cell_pos)) {
                           return false;
                         }
                         bool result = false;
                         auto* cell_ptr = sheet_.GetCell(cell_pos);
                         if (cell_ptr) {
                           auto* cell = dynamic_cast<Black::Cell*>(cell_ptr);
                           assert(cell); // only Black::Cell* supported
                           result = cell->CheckForCircularDependencyImpl(pos, checked);
                           checked.insert(cell_pos);
                         }
                         return result;
                       }
    );
  }

  void Cell::HandleInsertedRows(int before, int count) {
    std::for_each(std::begin(incoming_refs_), std::end(incoming_refs_),
                  [=] (Position& pos) { pos.row += count * (pos.row >= before); }
    );
    if (data_.IsFormula()) {

      data_.GetFormula()->HandleInsertedRows(before, count);
    }
  }

  void Cell::HandleInsertedCols(int before, int count) {
    std::for_each(std::begin(incoming_refs_), std::end(incoming_refs_),
                  [=] (Position& pos) { pos.col += count * (pos.col >= before); }
    );
    if (data_.IsFormula()) {
      data_.GetFormula()->HandleInsertedCols(before, count);
    }
  }

  void Cell::HandleDeletedRows(int first, int count) {
    incoming_refs_.erase(
      std::remove_if(std::begin(incoming_refs_), std::end(incoming_refs_),
                     [=] (Position pos) { return ValidateBoundaries(first, first + count, pos.row); }
      ),
      std::end(incoming_refs_)
    );
    if (data_.IsFormula()) {
      if (data_.GetFormula()->HandleDeletedRows(first, count)
        == IFormula::HandlingResult::ReferencesChanged)
      {
        InvalidateCache();
      }
    }
  }

  void Cell::HandleDeletedCols(int first, int count) {
    incoming_refs_.erase(
      std::remove_if(std::begin(incoming_refs_), std::end(incoming_refs_),
                     [=] (Position pos) { return ValidateBoundaries(first, first + count, pos.col); }
      ),
      std::end(incoming_refs_)
    );
    if (data_.IsFormula()) {
      if (data_.GetFormula()->HandleDeletedCols(first, count)
          == IFormula::HandlingResult::ReferencesChanged)
      {
        InvalidateCache();
      }
    }
  }

} // namespace Black
