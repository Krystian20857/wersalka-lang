//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_TOKENIZER_H
#define WERSALKALANG_TOKENIZER_H

#include <stack>

#include "absl/strings/charset.h"
#include "runtime/diagnostic.h"
#include "runtime/source.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

// clang-format off
enum class TokenKind {
  kUnknown = 0,
  kError, kBegin, kEnd,

  // literals
  kIdent, kIntLit, kFloatLit, kBoolLit, kNullLit,

  // symbols
  kOpenParen, kCloseParen,     // ()
  kOpenBracket, kCloseBracket, // []
  kOpenBrace, kCloseBrace,     // {}
  kOpenChev, kCloseChev,       // <>

  kEq, kBang,
  kEqEq, kBangEq, kGtEq, kLtEq,
  kPlus, kMinus, kStar, kSlash, kPercent,
  kAmp, kPipe, kCaret, kTilde,
  kAmpAmp, kPipePipe,
  kLtLt, kGtGt,
  kStarStar,
  kPlusEq, kMinusEq, kStarEq, kSlashEq, kPercentEq,
  kAmpEq, kPipeEq, kCaretEq, kLtLtEq, kGtGtEq,

  kSingleQuote, kDoubleQuote,
  kSemi, kComma, kDot,

  // template stuff
  kTemplateBegin, kTemplateEnd,
  kTemplateSegment, kTemplateExprBegin, kTemplateExprEnd,

  // keywords
  kVar, kIf, kWhile, kElse, kIn, kFor, kFunc, kReturn, kNew,

  kReserved
};

std::string_view GetTokenMnemonic(TokenKind kind);

inline std::ostream& operator<<(std::ostream& os, TokenKind kind) {
  return os << GetTokenMnemonic(kind);
}

enum class ValueKind {
  kNone, kNull, kUnsignedInt, kSignedInt,
  kFloat, kBool, kStrSegment, kIdent
};
// clang-format on

struct Token : ZoneObject {
  explicit Token(const TokenKind kind) : kind(kind), value_kind(), uint_v() {}

  TokenKind kind;
  TextSpan span;
  ValueKind value_kind;

  union {
    uint64_t uint_v;
    int64_t int_v;
    double float_v;
    bool bool_v;
    std::string_view str_v;
  };
};

class Tokenizer {
 public:
  Tokenizer(Zone* zone, DiagnosticReporter* diagnostic,
            const std::string_view source)
      : zone_(zone),
        diagnostic_(diagnostic),
        source_(source),
        pos_(0),
        record_start_(0),
        has_error_(false),
        current_(TokenKind::kBegin) {
    PushState(State::Kind::kNormal);
  }

  bool Next();
  const Token& current() const;

 private:
  struct State {
    enum class Kind { kNormal, kTemplate, kTemplateMultiline };

    Kind kind;
    int braces = 0;
  };

  bool NextNormal();
  bool NextTemplate(bool multiline);

  bool TryConsumeIdent();
  bool TryConsumeInt();
  bool TryConsumeSymbol();
  bool TryConsumeWhitespace();
  bool TryConsumeComment();

  bool TryConsumeTemplateBegin();
  bool TryConsumeTemplateSegment(bool multiline);

  inline void NextChar();
  inline char PeekChar(int offset = 0) const;
  inline bool At(const absl::CharSet& chars) const;
  inline bool At(std::string_view str) const;
  inline bool AtChar(char c) const;
  inline bool AtEnd() const;
  inline bool TryConsume(char c);
  inline void BeginToken();
  inline void EndToken();

  State& GetState();
  void PushState(State::Kind kind);
  void PopState();

  Zone* zone_;
  DiagnosticReporter* diagnostic_;
  std::string_view source_;
  int pos_, record_start_;
  bool has_error_;
  Token current_;
  std::stack<State> state_;
  std::string scratch_buffer_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_TOKENIZER_H
