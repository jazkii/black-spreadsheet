#pragma once
#include "common.h"
#include "formula.h"
#include "black_position.h"
#include "black_formula.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_set>

namespace Black {
  class Cell : public ICell {
    class Data : std::variant<std::string, std::unique_ptr<Black::Formula>> {
      using FormulaHolder = std::unique_ptr<Black::Formula>;
      using Base = std::variant<std::string, FormulaHolder>;
    public:
      using Base::Base;
      bool IsFormula() const {
        return std::holds_alternative<FormulaHolder>(*this);
      }
      bool IsText() const {
        return std::holds_alternative<std::string>(*this);
      }
      const std::string& GetText() const {
        return std::get<std::string>(*this);
      }

      const Formula* GetFormula() const {
        return std::get<FormulaHolder>(*this).get();
      }
      Formula* GetFormula()  {
        return std::get<FormulaHolder>(*this).get();
      }
    };

    ISheet& sheet_;
    Data data_;
    std::vector<Position> incoming_refs_;
    mutable std::optional<ICell::Value> value_cache_;

  private:
    bool CheckForCircularDependency(Position pos, bool has_incoming_refs) const;
    bool CheckForCircularDependencyImpl(Position pos,
                                        std::unordered_set<Position, Black::PositionHash>& checked) const;

  public:
    Cell(ISheet& sheet, Position pos, std::string text, bool has_incoming_refs);

    Value GetValue() const override;

    std::string GetText() const override;

    std::vector<Position> GetReferencedCells() const override;

    bool HasIncomingRefs() const;
    void AddIncomingRef(Position pos);
    void RemoveIncomingRef(Position pos);
    void SetIncomingReferences(std::vector<Position> refs);
    std::vector<Position> ReleaseIncomingReferences();

    bool Empty() const;
    void Clear();

    void InvalidateCache();

    void HandleInsertedRows(int before, int count);
    void HandleInsertedCols(int before, int count);

    void HandleDeletedRows(int first, int count);
    void HandleDeletedCols(int first, int count);
  };

} // namespace Black