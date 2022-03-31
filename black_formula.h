#pragma once

#include "formula.h"

#include <functional>
#include <optional>

namespace Black::FormulaAst {

  class Node : public IFormula {
  public:
    enum class Type {
      Number,
      Cell,
      UnaryPlus,
      UnaryMinus,
      Addition,
      Subtraction,
      Multiplication,
      Division
    };

    const Type type;
    Node(Type type) : type(type) {}
  };

  using NodeHolder = std::unique_ptr<Node>;

  class Number : public Node {
    double value_;
    const std::string str_representation_;
  public:
    Number(double value, std::string str_representation);

    Value Evaluate(const ISheet& sheet) const override;
    std::string GetExpression() const override;

    std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;
  };

  class Cell : public Node {
    Position position_;

    HandlingResult HandleInsertedImpl(int& dim, int before, int count);
    HandlingResult HandleDeletedImpl(int& dim, int first, int count);
  public:
    Cell(Position pos);

    Value Evaluate(const ISheet& sheet) const override;
    std::string GetExpression() const override;

    std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;
  };

  class UnaryOp : public Node {
    NodeHolder node_;
    const std::function<double(double)> unary_func_;

    static const char kOpSymbols[];
    char GetOpSymbol() const;
  public:
    UnaryOp(Node::Type type, NodeHolder holder,
            std::function<double(double)> unary_func);

    Value Evaluate(const ISheet& sheet) const override;
    std::string GetExpression() const override;

    std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;
  };

  class BinaryOp : public Node {
    NodeHolder left_;
    NodeHolder right_;
    const std::function<double(double, double)> binary_func_;

    static const char kOpSymbols[];
    char GetOpSymbol() const;

    enum class ChildNodePos {Left, Right};
    bool AreParenthesesNeeded(Type parent_type, Type child_type, ChildNodePos child_pos) const;
  public:
    BinaryOp(Node::Type type, NodeHolder left, NodeHolder right,
             std::function<double(double, double)> binary_func);

    Value Evaluate(const ISheet& sheet) const override;
    std::string GetExpression() const override;

    std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;
  };
} // namespace Black::FormulaAst

namespace Black {
class Formula : public IFormula {
    FormulaAst::NodeHolder node_;
    mutable std::optional<Value> value_cache_;
    mutable std::optional<std::string> expression_cache_;
    mutable std::optional<std::vector<Position>> referenced_cells_cache_;

  void HandleInsertionOrDeletion(HandlingResult result);
  public:
    Formula(FormulaAst::NodeHolder node);
    Value Evaluate(const ISheet& sheet) const override;
    std::string GetExpression() const override;

    std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;

    void InvalidateCache();
  };

  std::unique_ptr<Black::Formula> ParseFormula(std::string expression);
} // namespace Black
