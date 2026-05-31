//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/parser.h"

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace wersalka {
namespace lang {
namespace runtime {
namespace {
constexpr auto kSyncTokens =
    TokenSet{TokenKind::kOpenBrace, TokenKind::kCloseBrace,
             TokenKind::kOpenParen, TokenKind::kCloseParen, TokenKind::kSemi};
constexpr auto kUnaryExprBeginTokens = TokenSet{
    TokenKind::kPlus, TokenKind::kMinus, TokenKind::kBang, TokenKind::kTilde};
constexpr auto kConstExprBeginTokens =
    TokenSet{TokenKind::kIntLit, TokenKind::kFloatLit, TokenKind::kBoolLit,
             TokenKind::kNullLit};
constexpr auto kExprBeginTokens =
    kConstExprBeginTokens | kUnaryExprBeginTokens |
    TokenSet{TokenKind::kIdent, TokenKind::kOpenParen,
             TokenKind::kTemplateBegin};
constexpr auto kStmtBeginTokens =
    TokenSet{TokenKind::kVar, TokenKind::kOpenBrace, TokenKind::kIf,
             TokenKind::kWhile, TokenKind::kReturn} |
    kExprBeginTokens;
constexpr auto kTopLevelBeginTokens =
    TokenSet{TokenKind::kFunc} | kStmtBeginTokens;
constexpr bool IsRightAssoc(const TokenKind kind) {
  switch (kind) {
    case TokenKind::kEq:
    case TokenKind::kPlusEq:
    case TokenKind::kMinusEq:
    case TokenKind::kStarEq:
    case TokenKind::kSlashEq:
    case TokenKind::kPercentEq:
    case TokenKind::kAmpEq:
    case TokenKind::kPipeEq:
    case TokenKind::kCaretEq:
    case TokenKind::kLtLtEq:
    case TokenKind::kGtGtEq:
    case TokenKind::kStarStar:
      return true;
    default:
      return false;
  }
}

}  // namespace
std::string TokenSet::Format() const {
  std::vector<std::string_view> tokens;
  for (int n = 0; n < bitset_.size(); n++) {
    if (bitset_.test(n)) {
      const auto token_mnemonic = GetTokenMnemonic(static_cast<TokenKind>(n));
      const auto token_mnemonic_quoted =
          absl::StrFormat("`%s`", token_mnemonic);
      tokens.push_back(token_mnemonic_quoted);
    }
  }
  return absl::StrJoin(tokens, ", ");
}
ZonePtr<ASTNode> Parser::Parse(bool expr) {
  if (expr) {
    return ParseExpr(Precedence::kNone);
  }

  const auto mark = SpanBegin();
  ZonePtrList<ASTStmt> stmts(zone_);
  ZonePtrList<ASTFunctionDecl> functions(zone_);
  while (At(kTopLevelBeginTokens)) {
    if (At(kStmtBeginTokens)) {
      const auto stmt = ParseStmt();
      stmts.Add(zone_, stmt);
    } else if (At(TokenKind::kFunc)) {
      const auto func = ParseFuncDecl();
      functions.Add(zone_, func);
    }
  }

  return zone_->New<ASTCompileUnit>(SpanEnd(mark), functions, stmts);
}
ZonePtr<ASTFunctionDecl> Parser::ParseFuncDecl() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kFunc);
  const auto name = Expect(TokenKind::kIdent);
  Expect(TokenKind::kOpenParen);
  ZoneList<ZoneStr> params(zone_);
  while (At(TokenKind::kIdent)) {
    const auto ident = Next();
    params.Add(zone_, zone_->InternString(ident->str_v));
    if (At(TokenKind::kComma)) {
      Next();
    } else {
      break;
    }
  }
  Expect(TokenKind::kCloseParen);
  const auto block = ParseBlockStmt();
  return zone_->New<ASTFunctionDecl>(
      SpanEnd(mark), zone_->InternString(name->str_v), params, block);
}
ZonePtr<ASTExpr> Parser::ParseExpr(const Precedence precedence) {
  auto left = ParsePrimaryExpr();
  while (true) {
    const auto new_precedence = GetPrecedence(Peek()->kind);
    if (new_precedence == Precedence::kNone) {
      break;
    }
    const auto out = IsRightAssoc(Peek()->kind) ? new_precedence < precedence
                                                : new_precedence <= precedence;
    if (out) {
      break;
    }
    if (At(TokenKind::kOpenParen)) {
      left = ParseCallExpr(left);
    } else if (At(TokenKind::kOpenBracket)) {
      left = ParseArrayExpr(left);
    } else if (At(TokenKind::kDot)) {
      left = ParseMemberAccessExpr(left);
    } else {
      const auto op = Next();
      left = ParseBinarExpr(left, op);
    }
  }
  return left;
}
ZonePtr<ASTExpr> Parser::ParsePrimaryExpr() {
  const auto mark = SpanBegin();
  if (At(kConstExprBeginTokens)) {
    const auto token = Next();
    return zone_->New<ASTConstExpr>(token, SpanEnd(mark));
  }

  if (At(TokenKind::kOpenParen)) {
    return ParseGroupExpr();
  }

  if (At(kUnaryExprBeginTokens)) {
    return ParseUnaryExpr();
  }

  if (At(TokenKind::kIdent)) {
    return ParseIdentExpr();
  }

  if (At(TokenKind::kTemplateBegin)) {
    return ParseTemplateExpr();
  }

  if (At(TokenKind::kNew)) {
    return ParseNewExpr();
  }

  ABSL_UNREACHABLE();
}
ZonePtr<ASTExpr> Parser::ParseGroupExpr() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kOpenParen);
  const auto expr = ParseExpr(Precedence::kNone);
  Expect(TokenKind::kCloseParen);
  return zone_->New<ASTGroupExpr>(SpanEnd(mark), expr);
}
ZonePtr<ASTExpr> Parser::ParseBinarExpr(ZonePtr<ASTExpr> left,
                                        ZonePtr<Token> op_token) {
  switch (op_token->kind) {
    case TokenKind::kEq:
      return ParseAssignExpr(left, op_token, ASTAssignExpr::Operator::kAssign);
    case TokenKind::kPlusEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kAddAssign);
    case TokenKind::kMinusEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kSubAssign);
    case TokenKind::kStarEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kMulAssign);
    case TokenKind::kSlashEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kDivAssign);
    case TokenKind::kPercentEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kModAssign);
    case TokenKind::kAmpEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kAndAssign);
    case TokenKind::kPipeEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kOrAssign);
    case TokenKind::kCaretEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kXorAssign);
    case TokenKind::kLtLtEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kShlAssign);
    case TokenKind::kGtGtEq:
      return ParseAssignExpr(left, op_token,
                             ASTAssignExpr::Operator::kShrAssign);
    default:
      break;
  }

  ASTBinaryExpr::Operator op;
  switch (op_token->kind) {
    case TokenKind::kPlus:
      op = ASTBinaryExpr::Operator::kAdd;
      break;
    case TokenKind::kMinus:
      op = ASTBinaryExpr::Operator::kSub;
      break;
    case TokenKind::kStar:
      op = ASTBinaryExpr::Operator::kMul;
      break;
    case TokenKind::kSlash:
      op = ASTBinaryExpr::Operator::kDiv;
      break;
    case TokenKind::kPercent:
      op = ASTBinaryExpr::Operator::kMod;
      break;
    case TokenKind::kStarStar:
      op = ASTBinaryExpr::Operator::kExp;
      break;
    case TokenKind::kAmp:
      op = ASTBinaryExpr::Operator::kBitwiseAnd;
      break;
    case TokenKind::kPipe:
      op = ASTBinaryExpr::Operator::kBitwiseOr;
      break;
    case TokenKind::kCaret:
      op = ASTBinaryExpr::Operator::kBitwiseXor;
      break;
    case TokenKind::kLtLt:
      op = ASTBinaryExpr::Operator::kBitwiseShl;
      break;
    case TokenKind::kGtGt:
      op = ASTBinaryExpr::Operator::kBitwiseShr;
      break;
    case TokenKind::kAmpAmp:
      op = ASTBinaryExpr::Operator::kLogicAnd;
      break;
    case TokenKind::kPipePipe:
      op = ASTBinaryExpr::Operator::kLogicOr;
      break;
    case TokenKind::kEqEq:
      op = ASTBinaryExpr::Operator::kCmpEq;
      break;
    case TokenKind::kBangEq:
      op = ASTBinaryExpr::Operator::kCmpNe;
      break;
    case TokenKind::kOpenChev:
      op = ASTBinaryExpr::Operator::kCmpLt;
      break;
    case TokenKind::kCloseChev:
      op = ASTBinaryExpr::Operator::kCmpGt;
      break;
    case TokenKind::kLtEq:
      op = ASTBinaryExpr::Operator::kCmpLe;
      break;
    case TokenKind::kGtEq:
      op = ASTBinaryExpr::Operator::kCmpGe;
      break;
    default:
      ABSL_UNREACHABLE();
  }
  const auto right = ParseExpr(GetPrecedence(op_token->kind));
  const auto span = TextSpan::Merge(left->span(), right->span());
  return zone_->New<ASTBinaryExpr>(span, left, right, op);
}
ZonePtr<ASTExpr> Parser::ParseAssignExpr(ZonePtr<ASTExpr> target,
                                         ZonePtr<Token> op_token,
                                         ASTAssignExpr::Operator op) {
  const auto value = ParseExpr(GetPrecedence(op_token->kind));
  const auto span = TextSpan::Merge(target->span(), value->span());
  return zone_->New<ASTAssignExpr>(span, target, value, op);
}
ZonePtr<ASTExpr> Parser::ParseUnaryExpr() {
  ASTUnaryExpr::Operator op;
  switch (Peek()->kind) {
    case TokenKind::kPlus:
      op = ASTUnaryExpr::Operator::kPlus;
      break;
    case TokenKind::kMinus:
      op = ASTUnaryExpr::Operator::kNeg;
      break;
    case TokenKind::kBang:
      op = ASTUnaryExpr::Operator::kLogicNeg;
      break;
    case TokenKind::kTilde:
      op = ASTUnaryExpr::Operator::kBitwiseNeg;
      break;
    default:
      ABSL_UNREACHABLE();
  }
  const auto mark = SpanBegin();
  Next();  // op
  const auto expr = ParseExpr(Precedence::kNone);
  return zone_->New<ASTUnaryExpr>(SpanEnd(mark), expr, op);
}
ZonePtr<ASTExpr> Parser::ParseIdentExpr() {
  const auto mark = SpanBegin();
  const auto ident = Expect(TokenKind::kIdent);
  return zone_->New<ASTIdentExpr>(SpanEnd(mark),
                                  zone_->InternString(ident->str_v));
}
ZonePtr<ASTExpr> Parser::ParseCallExpr(ZonePtr<ASTExpr> left) {
  const auto mark = SpanBegin();
  Expect(TokenKind::kOpenParen);
  ZonePtrList<ASTExpr> args(zone_);
  while (!AtEnd() && !At(TokenKind::kCloseParen)) {
    const auto expr = ParseExpr(Precedence::kNone);
    args.Add(zone_, expr);
    if (At(TokenKind::kComma)) {
      Next();
    } else {
      break;
    }
  }
  Expect(TokenKind::kCloseParen);
  return zone_->New<ASTCallExpr>(SpanEnd(mark), left, args);
}
ZonePtr<ASTExpr> Parser::ParseArrayExpr(ZonePtr<ASTExpr> left) {
  const auto mark = SpanBegin();
  Expect(TokenKind::kOpenBracket);
  ZonePtrList<ASTExpr> args(zone_);
  while (!AtEnd() && !At(TokenKind::kCloseParen)) {
    const auto expr = ParseExpr(Precedence::kNone);
    args.Add(zone_, expr);
    if (At(TokenKind::kComma)) {
      Next();
    } else {
      break;
    }
  }
  Expect(TokenKind::kCloseBracket);
  return zone_->New<ASTArrayExpr>(SpanEnd(mark), left, args);
}
ZonePtr<ASTExpr> Parser::ParseTemplateExpr() {
  const auto mark = SpanBegin();
  ZoneList<ASTTemplateExpr::Segment> segments(zone_);
  Expect(TokenKind::kTemplateBegin);
  while (!AtEnd()) {
    if (At(TokenKind::kTemplateExprBegin)) {
      Next();
      const auto expr = ParseExpr(Precedence::kNone);
      segments.Add(
          zone_, ASTTemplateExpr::Segment{
                     .kind = ASTTemplateExpr::Segment::kExpr, .expr_v = expr});
      Expect(TokenKind::kTemplateExprEnd);
    } else if (At(TokenKind::kTemplateSegment)) {
      segments.Add(zone_, ASTTemplateExpr::Segment{
                              .kind = ASTTemplateExpr::Segment::kPart,
                              .str_v = zone_->InternString(Peek()->str_v)});
      Next();
    } else {
      break;
    }
  }
  Expect(TokenKind::kTemplateEnd);
  return zone_->New<ASTTemplateExpr>(SpanEnd(mark), segments);
}
ZonePtr<ASTExpr> Parser::ParseNewExpr() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kNew);

  if (TryConsume(TokenKind::kOpenBracket)) {
    ZonePtrList<ASTExpr> elements(zone_);
    while (At(kExprBeginTokens)) {
      const auto expr = ParseExpr(Precedence::kNone);
      elements.Add(zone_, expr);
      if (At(TokenKind::kComma)) {
        Next();
      } else {
        break;
      }
    }
    Expect(TokenKind::kCloseBracket);
    return zone_->New<ASTNewArrayExpr>(SpanEnd(mark), elements);
  }

  if (TryConsume(TokenKind::kOpenBrace)) {
    Expect(TokenKind::kCloseBrace);
    return zone_->New<ASTNewObjectExpr>(SpanEnd(mark));
  }

  ABSL_UNREACHABLE();
}
ZonePtr<ASTExpr> Parser::ParseMemberAccessExpr(ZonePtr<ASTExpr> left) {
  Expect(TokenKind::kDot);
  const auto mark = SpanBegin();
  const auto field = Expect(TokenKind::kIdent);
  return zone_->New<ASTMemberAccessExpr>(
      TextSpan::Merge(left->span(), SpanEnd(mark)), left,
      zone_->InternString(field->str_v));
}
ZonePtr<ASTStmt> Parser::ParseStmt() {
  if (At(TokenKind::kVar)) {
    return ParseVarStmt();
  }
  if (At(TokenKind::kOpenBrace)) {
    return ParseBlockStmt();
  }
  if (At(TokenKind::kIf)) {
    return ParseIfStmt();
  }
  if (At(TokenKind::kWhile)) {
    return ParseWhileStmt();
  }
  if (At(kExprBeginTokens)) {
    return ParseExprStmt();
  }
  if (At(TokenKind::kReturn)) {
    return ParseReturnStmt();
  }

  ABSL_UNREACHABLE();
}
ZonePtr<ASTStmt> Parser::ParseBlockStmt() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kOpenBrace);
  ZonePtrList<ASTStmt> stmts(zone_);
  while (At(kStmtBeginTokens)) {
    const auto stmt = ParseStmt();
    stmts.Add(zone_, stmt);
    while (At(TokenKind::kSemi)) {
      Next();
    }
  }
  Expect(TokenKind::kCloseBrace);
  return zone_->New<ASTBlockStmt>(SpanEnd(mark), stmts);
}
ZonePtr<ASTStmt> Parser::ParseVarStmt() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kVar);
  const auto ident = Expect(TokenKind::kIdent);
  std::optional<ZonePtr<ASTExpr>> init_expr;
  if (At(TokenKind::kEq)) {
    Next();
    init_expr = ParseExpr(Precedence::kNone);
  } else {
    init_expr = std::nullopt;
  }
  Expect(TokenKind::kSemi);
  return zone_->New<ASTVarStmt>(SpanEnd(mark),
                                zone_->InternString(ident->str_v), init_expr);
}
ZonePtr<ASTStmt> Parser::ParseIfStmt() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kIf);
  const auto condition = ParseExpr(Precedence::kNone);
  const auto then = ParseBlockStmt();
  std::optional<ZonePtr<ASTIfStmt>> alternate;
  std::optional<ZonePtr<ASTStmt>> else_block;
  if (TryConsume(TokenKind::kElse)) {
    if (At(TokenKind::kIf)) {
      alternate = static_cast<ASTIfStmt*>(ParseIfStmt());
    } else {
      else_block = ParseBlockStmt();
    }
  } else {
    alternate = std::nullopt;
  }
  return zone_->New<ASTIfStmt>(SpanEnd(mark), condition, then, alternate,
                               else_block);
}
ZonePtr<ASTStmt> Parser::ParseWhileStmt() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kWhile);
  const auto condition = ParseExpr(Precedence::kNone);
  const auto block = ParseBlockStmt();
  return zone_->New<ASTWhileStmt>(SpanEnd(mark), condition, block);
}
ZonePtr<ASTStmt> Parser::ParseExprStmt() {
  const auto mark = SpanBegin();
  const auto expr = ParseExpr(Precedence::kNone);
  Expect(TokenKind::kSemi);
  return zone_->New<ASTExprStmt>(SpanEnd(mark), expr);
}
ZonePtr<ASTStmt> Parser::ParseReturnStmt() {
  const auto mark = SpanBegin();
  Expect(TokenKind::kReturn);
  const auto expr = ParseExpr(Precedence::kNone);
  Expect(TokenKind::kSemi);
  return zone_->New<ASTReturnStmt>(SpanEnd(mark), expr);
}
ZonePtr<Token> Parser::Peek(int offset) {
  while (pos_ + offset >= tokens_.size()) {
    tokenizer_->Next();
    const auto& current = tokenizer_->current();
    if (current.kind == TokenKind::kEnd) {
      return zone_->InternReference(current);
    }
    tokens_.Add(zone_, zone_->InternReference(current));
  }
  return tokens_[pos_ + offset];
}
ZonePtr<Token> Parser::Next() {
  const auto token = Peek();
  pos_++;
  return token;
}
bool Parser::At(const TokenSet& tokens) {
  return tokens.Contains(Peek()->kind);
}
bool Parser::TryConsume(const TokenSet& tokens) {
  if (At(tokens)) {
    Next();
    return true;
  }
  return false;
}
ZonePtr<Token> Parser::Expect(const TokenSet& tokens) {
  if (At(tokens)) {
    return Next();
  }
  reporter_->Report(
      Diagnostic::Error("Unexpected token")
          .WithLabel(Peek()->span,
                     absl::StrFormat("Got `%s` expected %s",
                                     GetTokenMnemonic(Peek()->kind),
                                     tokens.Format())));
  Synchronize();
  return zone_->New<Token>(TokenKind::kError);
}
void Parser::Synchronize() {
  while (!At(kSyncTokens) && !AtEnd()) {
    Next();
  }
}
int Parser::SpanBegin() { return Peek()->span.offset; }
TextSpan Parser::SpanEnd(int mark) const {
  const auto& last = tokens_[pos_ - 1];
  return TextSpan{mark, last->span.offset + last->span.length - mark};
}
Parser::Precedence Parser::GetPrecedence(TokenKind kind) {
  switch (kind) {
    case TokenKind::kEq:
    case TokenKind::kPlusEq:
    case TokenKind::kMinusEq:
    case TokenKind::kStarEq:
    case TokenKind::kSlashEq:
    case TokenKind::kPercentEq:
    case TokenKind::kAmpEq:
    case TokenKind::kPipeEq:
    case TokenKind::kCaretEq:
    case TokenKind::kLtLtEq:
    case TokenKind::kGtGtEq:
      return Precedence::kAssignment;
    case TokenKind::kPipePipe:
      return Precedence::kLogicOr;
    case TokenKind::kAmpAmp:
      return Precedence::kLogicAnd;
    case TokenKind::kPipe:
      return Precedence::kBitwiseOr;
    case TokenKind::kCaret:
      return Precedence::kBitwiseXor;
    case TokenKind::kAmp:
      return Precedence::kBitwiseAnd;
    case TokenKind::kEqEq:
    case TokenKind::kBangEq:
      return Precedence::kEquality;
    case TokenKind::kOpenChev:
    case TokenKind::kCloseChev:
    case TokenKind::kLtEq:
    case TokenKind::kGtEq:
      return Precedence::kComparison;
    case TokenKind::kLtLt:
    case TokenKind::kGtGt:
      return Precedence::kShift;
    case TokenKind::kPlus:
    case TokenKind::kMinus:
      return Precedence::kAdditive;
    case TokenKind::kStar:
    case TokenKind::kSlash:
    case TokenKind::kPercent:
      return Precedence::kMultiplicative;
    case TokenKind::kStarStar:
      return Precedence::kExponent;
    case TokenKind::kOpenParen:
    case TokenKind::kOpenBracket:
    case TokenKind::kDot:
      return Precedence::kPostfix;

    default:
      return Precedence::kNone;
  }
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
