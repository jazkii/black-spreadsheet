#include "black_formula.h"
#include "common.h"
#include "black_utils.h"

#include "FormulaLexer.h"
#include "FormulaBaseListener.h"

#include <cmath>
#include <algorithm>
#include <iterator>
#include <stack>
#include <sstream>

namespace Black::FormulaAst {
  std::string ExprPrinter(std::string expr, bool in_parentheses) {
    std::string result;

    if (in_parentheses) {
      result.reserve(1 + expr.size() + 1);
      result += '(';
      result += expr;
      result += ')';
    } else {
      result = std::move(expr);
    }

    return result;
  }

  Number::Number(double value, std::string str_representation)
  : Node(Type::Number)
  , value_(value)
  , str_representation_(std::move(str_representation))
  {
  }

  IFormula::Value Number::Evaluate(const ISheet& sheet) const {
    return value_;
  }

  std::string Number::GetExpression() const {
    return str_representation_;
  }

  std::vector<Position> Number::GetReferencedCells() const {
    return {};
  }

  IFormula::HandlingResult Number::HandleInsertedRows(int before, int count) {
    return IFormula::HandlingResult::NothingChanged;
  }

  IFormula::HandlingResult Number::HandleInsertedCols(int before, int count) {
    return IFormula::HandlingResult::NothingChanged;
  }

  IFormula::HandlingResult Number::HandleDeletedRows(int first, int count) {
    return IFormula::HandlingResult::NothingChanged;
  }

  IFormula::HandlingResult Number::HandleDeletedCols(int first, int count) {
    return IFormula::HandlingResult::NothingChanged;
  }


  Cell::Cell(Position pos)
  : Node(Type::Cell)
  , position_(pos)
  {
  }

  struct CellEvaluater {
    IFormula::Value operator() (double value) const {
      return value;
    }
    IFormula::Value operator() (FormulaError error) const {
      return error;
    }
    IFormula::Value operator() (const std::string& text) const {
      if (text.empty()) return 0.0;

      try {
        size_t pos = 0;
        double result = std::stod(text, &pos); // TODO: check if str is fully number
        if (pos == text.size()) {
          return result;
        }
      } catch (...) {}

      return FormulaError(FormulaError::Category::Value);
    }
  };

  IFormula::Value Cell::Evaluate(const ISheet& sheet) const {
    if (position_.IsValid()){
      auto* cell = sheet.GetCell(position_);
      if (cell) {
        return std::visit(CellEvaluater{}, cell->GetValue());
      }
      return 0.0;
    }
    return FormulaError(FormulaError::Category::Ref);
  }

  std::string Cell::GetExpression() const {
    if (position_.IsValid()) {
      return position_.ToString();
    }
    return std::string(FormulaError(FormulaError::Category::Ref).ToString());
  }

  std::vector<Position> Cell::GetReferencedCells() const {
    std::vector<Position> result;
    if (position_.IsValid()) {
      result.push_back(position_);
    }
    return result;
  }

  IFormula::HandlingResult Cell::HandleInsertedImpl(int& dim, int before, int count) {
    if (position_.IsValid() && dim >= before) {
      dim += count;
      return HandlingResult::ReferencesRenamedOnly;
    }
    return HandlingResult::NothingChanged;
  }

  IFormula::HandlingResult Cell::HandleDeletedImpl(int& dim, int first, int count) {
    if (position_.IsValid() && dim >= first) {
      if (dim < first + count) {
        position_ = {-1, -1}; // invalid position
        return HandlingResult::ReferencesChanged;
      }
      dim -= count;
      return HandlingResult::ReferencesRenamedOnly;
    }
    return HandlingResult::NothingChanged;
  }

  IFormula::HandlingResult Cell::HandleInsertedRows(int before, int count) {
    return HandleInsertedImpl(position_.row, before, count);
  }

  IFormula::HandlingResult Cell::HandleInsertedCols(int before, int count) {
    return HandleInsertedImpl(position_.col, before, count);
  }

  IFormula::HandlingResult Cell::HandleDeletedRows(int first, int count) {
    return HandleDeletedImpl(position_.row, first, count);
  }

  IFormula::HandlingResult Cell::HandleDeletedCols(int first, int count) {
    return HandleDeletedImpl(position_.col, first, count);
  }

  struct UnaryOpEvaluater {
    IFormula::Value operator() (double value,
                                const std::function<double(double)>& unary_func) const {
      return unary_func(value);
    }
    IFormula::Value operator() (FormulaError error,
                                const std::function<double(double)>& /* unary_func */) const {
      return error;
    }
  };

  const char UnaryOp::kOpSymbols[] = {'+', '-'};

  char UnaryOp::GetOpSymbol() const {
    return kOpSymbols[static_cast<size_t>(type) - static_cast<size_t>(Type::UnaryPlus)];
  }

  UnaryOp::UnaryOp(Node::Type type, NodeHolder holder,
                   std::function<double(double)> unary_func)
    : Node(type)
    , node_(std::move(holder))
    , unary_func_(std::move(unary_func))
  {
  }

  IFormula::Value UnaryOp::Evaluate(const ISheet& sheet) const {
    return std::visit(
      [&] (auto value) { return UnaryOpEvaluater{}(value, unary_func_) ; },
      node_->Evaluate(sheet)
    );
  }

  std::string UnaryOp::GetExpression() const {
    return GetOpSymbol()
      + ExprPrinter(
        node_->GetExpression(),
        node_->type == Type::Addition || node_->type == Type::Subtraction
      );
  }

  std::vector<Position> UnaryOp::GetReferencedCells() const {
    return node_->GetReferencedCells();
  }

  IFormula::HandlingResult UnaryOp::HandleInsertedRows(int before, int count) {
    return node_->HandleInsertedRows(before, count);
  }

  IFormula::HandlingResult UnaryOp::HandleInsertedCols(int before, int count) {
    return node_->HandleInsertedCols(before, count);
  }

  IFormula::HandlingResult UnaryOp::HandleDeletedRows(int first, int count) {
    return node_->HandleDeletedRows(first, count);
  }

  IFormula::HandlingResult UnaryOp::HandleDeletedCols(int first, int count) {
    return node_->HandleDeletedCols(first, count);
  }

  struct BinaryOpEvaluater {
    template <typename T>
    IFormula::Value operator() (FormulaError error, T /* val */,
                                const std::function<double(double, double)>& /*binary_func*/) const {
      return error;
    }

    template <typename T>
    IFormula::Value operator() (T /* val */, FormulaError error,
                                const std::function<double(double, double)>& /*binary_func*/) const {
      return error;
    }

    IFormula::Value operator() (FormulaError error , FormulaError /* error 2 */,
                                const std::function<double(double, double)>& /*binary_func*/) const {
      return error;
    }

    IFormula::Value operator() (double left, double right,
                                const std::function<double(double, double)>& binary_func) const {
      double result = binary_func(left, right);
      if (std::isfinite(result)) {
        return result;
      }
      return FormulaError(FormulaError::Category::Div0);
    }
  };

  const char BinaryOp::kOpSymbols[] = {'+', '-', '*', '/'};

  char BinaryOp::GetOpSymbol() const {
    return kOpSymbols[static_cast<size_t>(type) - static_cast<size_t>(Type::Addition)];
  }

  BinaryOp::BinaryOp(Node::Type type, NodeHolder left, NodeHolder right,
                     std::function<double(double, double)> binary_func)
  : Node(type)
  , left_(std::move(left))
  , right_(std::move(right))
  , binary_func_(std::move(binary_func))
  {
  }

  IFormula::Value BinaryOp::Evaluate(const ISheet& sheet) const {
    return std::visit(
      [&] (auto left, auto right) {return BinaryOpEvaluater{}(left, right, binary_func_); },
      left_->Evaluate(sheet),
      right_->Evaluate(sheet)
    );
  }

  bool BinaryOp::AreParenthesesNeeded(Node::Type parent_type, Node::Type child_type,
                                      BinaryOp::ChildNodePos child_pos) const {
    switch (parent_type) {
    case Type::Subtraction:
      return (child_type == Type::Addition ||
                child_type == Type::Subtraction) &&
              child_pos == ChildNodePos::Right;
    case Type::Multiplication:
      return child_type == Type::Addition ||
             child_type == Type::Subtraction;
    case Type::Division:
      return child_type == Type::Addition ||
             child_type == Type::Subtraction ||
             ((child_type == Type::Multiplication ||
               child_type == Type::Division)
             && child_pos == ChildNodePos::Right);
    default:
      return false;
    }
  }

  std::string BinaryOp::GetExpression() const {
    return ExprPrinter(left_->GetExpression(), AreParenthesesNeeded(type, left_->type, ChildNodePos::Left))
      + GetOpSymbol()
      + ExprPrinter(right_->GetExpression(), AreParenthesesNeeded(type, right_->type, ChildNodePos::Right));
  }

  std::vector<Position> BinaryOp::GetReferencedCells() const {
    const auto left_refs = left_->GetReferencedCells();
    const auto right_refs = right_->GetReferencedCells();
    std::vector<Position> result;
    result.reserve(left_refs.size() + right_refs.size());

    std::merge(std::begin(left_refs), std::end(left_refs),
               std::begin(right_refs), std::end(right_refs),
               std::back_inserter(result)
    );
    result.erase(std::unique(std::begin(result), std::end(result)), std::end(result));

    return result;
  }

  IFormula::HandlingResult BinaryOp::HandleInsertedRows(int before, int count) {
    return std::max(
      left_->HandleInsertedRows(before, count),
      right_->HandleInsertedRows(before, count)
    );
  }

  IFormula::HandlingResult BinaryOp::HandleInsertedCols(int before, int count) {
    return std::max(
      left_->HandleInsertedCols(before, count),
      right_->HandleInsertedCols(before, count)
    );
  }

  IFormula::HandlingResult BinaryOp::HandleDeletedRows(int first, int count) {
    return std::max(
      left_->HandleDeletedRows(first, count),
      right_->HandleDeletedRows(first, count)
    );
  }

  IFormula::HandlingResult BinaryOp::HandleDeletedCols(int first, int count) {
    return std::max(
      left_->HandleDeletedCols(first, count),
      right_->HandleDeletedCols(first, count)
    );
  }

  class Listener : public FormulaBaseListener {
    std::stack<NodeHolder> nodes_;

    NodeHolder PopNode() {
      auto node = std::move(nodes_.top());
      nodes_.pop();
      return node;
    }
  public:
    void exitLiteral(FormulaParser::LiteralContext * ctx) override {
      std::string str = ctx->NUMBER()->getSymbol()->getText();
      double value = std::stod(str);
      if (!std::isfinite(value)) {
        throw FormulaException(
          "Number literal " + str + " can't be represented as a floating point number."
        );
      }
      nodes_.push(
        std::make_unique<Number>(value, std::move(str))
      );
    }

    void exitCell(FormulaParser::CellContext * ctx) override {
      auto pos = Position::FromString(ctx->CELL()->getSymbol()->getText());
      if (!pos.IsValid()) {
        throw FormulaException(
          "Invalid cell position: " + ctx->CELL()->getSymbol()->getText() + "."
        );
      }
      nodes_.push(
        std::make_unique<Cell>(pos)
      );
    }

    void exitUnaryOp(FormulaParser::UnaryOpContext * ctx) override {
      auto node = PopNode();
      Node::Type type;
      std::function<double(double)> unary_func;
      if (ctx->SUB()) {
        type = Node::Type::UnaryMinus;
        unary_func = std::negate<double>{};
      } else {
        type = Node::Type::UnaryPlus;
        unary_func = [] (double val) { return val; };
      }

      nodes_.push(
        std::make_unique<UnaryOp>(type, std::move(node), std::move(unary_func))
      );
    }

    void exitBinaryOp(FormulaParser::BinaryOpContext * ctx) override {
      auto right = PopNode();
      auto left = PopNode();
      Node::Type type;
      std::function<double(double, double)> binary_func;

      if (ctx->MUL()) {
        type = Node::Type::Multiplication;
        binary_func = std::multiplies<double>{};
      } else if (ctx->DIV()) {
        type = Node::Type::Division;
        binary_func = std::divides<double>{};
      } else if (ctx->ADD()) {
        type = Node::Type::Addition;
        binary_func = std::plus<double>{};
      } else {
        type = Node::Type::Subtraction;
        binary_func = std::minus<double>{};
      }

      nodes_.push(
        std::make_unique<BinaryOp>(type, std::move(left), std::move(right), std::move(binary_func))
      );
    }

    std::unique_ptr<Black::Formula> GetResult() {
      return std::make_unique<Black::Formula>(PopNode());
    }
  };

  class BailErrorListener : public antlr4::BaseErrorListener {
  public:
    void syntaxError(antlr4::Recognizer* /* recognizer */,
                     antlr4::Token* /* offendingSymbol */, size_t /* line */,
                     size_t /* charPositionInLine */, const std::string& msg,
                     std::exception_ptr /* e */
    ) override {
      throw std::runtime_error("Error when lexing: " + msg);
    }
  };

} // namespace Black::FormulaAst

namespace Black {
  Formula::Formula(Black::FormulaAst::NodeHolder node)
    : node_(std::move(node))
  {
  }

  IFormula::Value Formula::Evaluate(const ISheet& sheet) const {
    if (!value_cache_) {
      value_cache_ = node_->Evaluate(sheet);
    }
    return *value_cache_;
  }

  std::string Formula::GetExpression() const {
    if (!expression_cache_) {
      expression_cache_ = node_->GetExpression();
    }
    return *expression_cache_;
  }

  std::vector<Position> Formula::GetReferencedCells() const {
    if (!referenced_cells_cache_) {
      referenced_cells_cache_ = node_->GetReferencedCells();
    }
    return *referenced_cells_cache_;
  }

  void Formula::HandleInsertionOrDeletion(IFormula::HandlingResult result) {
    if (result >= IFormula::HandlingResult::ReferencesRenamedOnly) {
      expression_cache_ = std::nullopt;
      referenced_cells_cache_ = std::nullopt;
    }
    if (result == IFormula::HandlingResult::ReferencesChanged) {
      value_cache_ = std::nullopt;
    }

  }

  IFormula::HandlingResult Formula::HandleInsertedRows(int before, int count) {
    auto result = node_->HandleInsertedRows(before, count);
    HandleInsertionOrDeletion(result);
    return result;
  }

  IFormula::HandlingResult Formula::HandleInsertedCols(int before, int count) {
    auto result = node_->HandleInsertedCols(before, count);
    HandleInsertionOrDeletion(result);
    return result;
  }

  IFormula::HandlingResult Formula::HandleDeletedRows(int first, int count) {
    auto result = node_->HandleDeletedRows(first, count);
    HandleInsertionOrDeletion(result);
    return result;
  }

  IFormula::HandlingResult Formula::HandleDeletedCols(int first, int count) {
    auto result = node_->HandleDeletedCols(first, count);
    HandleInsertionOrDeletion(result);
    return result;
  }

  void Formula::InvalidateCache() {
    value_cache_ = std::nullopt;
    expression_cache_ = std::nullopt;
    referenced_cells_cache_ = std::nullopt;
  }

  std::unique_ptr<Black::Formula> ParseFormula(std::string expression) {
    try {
      antlr4::ANTLRInputStream input(expression);

      FormulaLexer lexer(&input);
      Black::FormulaAst::BailErrorListener error_listener;
      lexer.removeErrorListeners();
      lexer.addErrorListener(&error_listener);

      antlr4::CommonTokenStream tokens(&lexer);

      FormulaParser parser(&tokens);
      auto error_handler = std::make_shared<antlr4::BailErrorStrategy>();
      parser.setErrorHandler(error_handler);
      parser.removeErrorListeners();

      antlr4::tree::ParseTree* tree = parser.main();  // метод соответствует корневому правилу
      Black::FormulaAst::Listener listener;
      antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

      return listener.GetResult();
    } catch(...) {
      std::throw_with_nested(FormulaException(expression));
    }
  }

} // namespace Black

std::unique_ptr<IFormula> ParseFormula(std::string expression) {
  return Black::ParseFormula(std::move(expression));
}
