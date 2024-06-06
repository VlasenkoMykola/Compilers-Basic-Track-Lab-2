#include "ast_dumper.hh"
#include "../utils/errors.hh"

namespace {

char const * const get_type_name(ast::Type t) {
  switch (t) {
    case t_int:
      return "int";
    case t_string:
      return "string";
    default:
       utils::error("internal error: attempting to print the type of t_void or t_undef");
  }
}

} // namespace


namespace ast {

void ASTDumper::visit(const IntegerLiteral &literal) {
  *ostream << literal.value;
}

void ASTDumper::visit(const StringLiteral &literal) {
  *ostream << '"';
  for (auto &c : static_cast<std::string>(literal.value)) {
    switch (c) {
    case '"':
      *ostream << "\\\"";
      break;
    case '\\':
      *ostream << "\\\\";
      break;
    case '\a':
      *ostream << "\\a";
      break;
    case '\b':
      *ostream << "\\b";
      break;
    case '\t':
      *ostream << "\\t";
      break;
    case '\n':
      *ostream << "\\n";
      break;
    case '\v':
      *ostream << "\\v";
      break;
    case '\f':
      *ostream << "\\f";
      break;
    case '\r':
      *ostream << "\\r";
      break;
    default:
      *ostream << c;
    }
  }
  *ostream << '"';
}

void ASTDumper::visit(const BinaryOperator &binop) {
  *ostream << '(';
  binop.get_left().accept(*this);
  *ostream << operator_name[binop.op];
  binop.get_right().accept(*this);
  *ostream << ')';
}

void ASTDumper::visit(const Sequence &seqExpr) {
  *ostream << "(";
  inc();
  const auto exprs = seqExpr.get_exprs();
  for (auto expr = exprs.cbegin(); expr != exprs.cend(); expr++) {
    if (expr != exprs.cbegin())
      *ostream << ';';
    nl();
    (*expr)->accept(*this);
  }
  dnl();
  *ostream << ")";
}

void ASTDumper::visit(const Let &let) {
  *ostream << "let";
  inc();
  for (auto decl : let.get_decls()) {
    nl();
    decl->accept(*this);
  }
  dnl();
  *ostream << "in";
  inc();
  const auto exprs = let.get_sequence().get_exprs();
  for (auto expr = exprs.cbegin(); expr != exprs.cend(); expr++) {
    if (expr != exprs.cbegin())
      *ostream << ';';
    nl();
    (*expr)->accept(*this);
  }
  dnl();
  *ostream << "end";
}

void ASTDumper::visit(const Identifier &id) {
  *ostream << id.name;
  if (verbose)
    if (auto decl = id.get_decl()) {
      *ostream << "/*" << "decl:" << decl.get().loc;

      if (int depth_diff = id.get_depth() - decl->get_depth())
        *ostream << " depth_diff:" << depth_diff;
      *ostream << "*/";
    }
}

void ASTDumper::visit(const IfThenElse &ite) {
  *ostream << "if ";
  inl();
  ite.get_condition().accept(*this);
  dnl();
  *ostream << " then ";
  inl();
  ite.get_then_part().accept(*this);
  dnl();
  *ostream << " else ";
  inl();
  ite.get_else_part().accept(*this);
  dec();
}

void ASTDumper::visit(const VarDecl &decl) {
  if (decl.get_expr())
    *ostream << "var ";
  *ostream << decl.name;
  if (verbose && decl.get_escapes())
    *ostream << "/*e*/";
  if (decl.type_name)
    *ostream << ": " << *decl.type_name;
  else {
    auto t = decl.get_type();
    if (t != t_undef && t != t_void)
      *ostream << ": " << get_type_name(t);
  }
  if (auto expr = decl.get_expr()) {
    *ostream << " := ";
    expr->accept(*this);
  }
}

void ASTDumper::visit(const FunDecl &decl) {
  *ostream << "function " << decl.name;
  if (verbose && decl.name != decl.get_external_name())
    *ostream << "/*" << decl.get_external_name() << "*/";
  *ostream << '(';
  auto params = decl.get_params();
  for (auto param = params.cbegin(); param != params.cend(); param++) {
    if (param != params.cbegin())
      *ostream << ", ";
    (*param)->accept(*this);
  }
  *ostream << ")";
  if (decl.type_name)
    *ostream << ": " << decl.type_name.get();
  *ostream << " = ";
  inl();
  decl.get_expr()->accept(*this);
  dec();
}

void ASTDumper::visit(const FunCall &call) {
  *ostream << call.func_name;
  if (verbose)
    if (auto decl = call.get_decl())
      *ostream << "/*" << "decl:" << decl.get().loc << "*/";

  *ostream << "(";

  auto args = call.get_args();
  for (auto arg = args.cbegin(); arg != args.cend(); arg++) {
    if (arg != args.cbegin())
      *ostream << ", ";
    (*arg)->accept(*this);
  }
  *ostream << ')';
}

void ASTDumper::visit(const WhileLoop &loop) {
  *ostream << "while ";
  loop.get_condition().accept(*this);
  *ostream << " do";
  inl();
  loop.get_body().accept(*this);
  dec();
}

void ASTDumper::visit(const ForLoop &loop) {
  *ostream << "for " << loop.get_variable().name;
  if (verbose && loop.get_variable().get_escapes())
    *ostream << "/*e*/";
  *ostream << " := ";
  loop.get_variable().get_expr()->accept(*this);
  *ostream << " to ";
  loop.get_high().accept(*this);
  *ostream << " do";
  inl();
  loop.get_body().accept(*this);
  dec();
}

void ASTDumper::visit(const Break &brk) {
  *ostream << "break";
  if (verbose && brk.get_loop())
    *ostream << "/*loop:" << brk.get_loop().get().loc << "*/";
}

void ASTDumper::visit(const Assign &assign) {
  assign.get_lhs().accept(*this);
  *ostream << " := ";
  assign.get_rhs().accept(*this);
}

// =====================================================

void ASTEval::nl() {
    int eval = stack[stack.size()-1];
    *ostream << eval;
    *ostream << std::endl;
}

void ASTEval::visit(const IntegerLiteral &literal) {
    stack.push_back(literal.value);
}

void ASTEval::visit(const BinaryOperator &binop) {
  binop.get_left().accept(*this);
  binop.get_right().accept(*this);
  if (stack.size() < 2) {
        utils::error("Evaluate: error: stack error");
  }
  int eval;
  switch(binop.op)
  {
  case o_plus:
      eval = stack[stack.size()-2] + stack[stack.size()-1];
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_minus:
      eval = stack[stack.size()-2] - stack[stack.size()-1];
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_times:
      eval = stack[stack.size()-2] * stack[stack.size()-1];
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_divide:
      eval = stack[stack.size()-2] / stack[stack.size()-1];
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_eq:
      eval = ((stack[stack.size()-2] == stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_neq:
      eval = ((stack[stack.size()-2] != stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_lt:
      eval = ((stack[stack.size()-2] < stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_le:
      eval = ((stack[stack.size()-2] <= stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_gt:
      eval = ((stack[stack.size()-2] > stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  case o_ge:
      eval = ((stack[stack.size()-2] >= stack[stack.size()-1]) ? 1 : 0);
      stack.pop_back();
      stack.pop_back();
      stack.push_back( eval );
      break;
  }
}

void ASTEval::visit(const Sequence &seqExpr) {
  const auto exprs = seqExpr.get_exprs();
  for (auto expr = exprs.cbegin(); expr != exprs.cend(); expr++) {
    (*expr)->accept(*this);
  }
}

void ASTEval::visit(const IfThenElse &ite) {
  //*ostream << "if ";
  ite.get_condition().accept(*this);
  int eval = stack[stack.size()-1];
  stack.pop_back();
  if (eval) {
      //*ostream << " then ";
      ite.get_then_part().accept(*this);
  } else {
      //*ostream << " else ";
      ite.get_else_part().accept(*this);
  }
}

void ASTEval::visit(const StringLiteral &literal) {
    utils::error("Evaluate: unsupported: StringLiteral");
}

void ASTEval::visit(const Let &let) {
    utils::error("Evaluate: unsupported: Let");
}

void ASTEval::visit(const Identifier &id) {
    utils::error("Evaluate: unsupported: Identifier");
}

void ASTEval::visit(const VarDecl &decl) {
    utils::error("Evaluate: unsupported: VarDecl");
}

void ASTEval::visit(const FunDecl &decl) {
    utils::error("Evaluate: unsupported: FunDecl");
}

void ASTEval::visit(const FunCall &call) {
    utils::error("Evaluate: unsupported: FunCall");
}

void ASTEval::visit(const WhileLoop &loop) {
    utils::error("Evaluate: unsupported: WhileLoop");
}

void ASTEval::visit(const ForLoop &loop) {
    utils::error("Evaluate: unsupported: ForLoop");
}

void ASTEval::visit(const Break &brk) {
    utils::error("Evaluate: unsupported: Break");
}

void ASTEval::visit(const Assign &assign) {
    utils::error("Evaluate: unsupported: Assign");
}

} // namespace ast
