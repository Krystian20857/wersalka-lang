//
// Created by nothingbutyou on 5/1/26.
//

#include "tokenizer.h"

#include <algorithm>
#include <string>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace wersalka {
namespace lang {
namespace runtime {

namespace {

using enum TokenKind;

constexpr auto kEof = '\0';
constexpr absl::CharSet kDigits =
    absl::CharSet::AsciiDigits() | absl::CharSet::Char('_');
constexpr absl::CharSet kHexDigits =
    kDigits | absl::CharSet::Range('a', 'f') | absl::CharSet::Range('A', 'F');
constexpr absl::CharSet kBinDigits = absl::CharSet::Range('0', '1');
constexpr absl::CharSet kAlpha = absl::CharSet::AsciiAlphabet();
constexpr absl::CharSet kIdentBegin = kAlpha | absl::CharSet::Char('_');
constexpr absl::CharSet kIdentSegment =
    kDigits | kAlpha | absl::CharSet::Char('_');
constexpr absl::CharSet kNewLine = absl::CharSet::Char('\n');
constexpr absl::CharSet kWhitespace =
    absl::CharSet::AsciiWhitespace() & ~kNewLine;
const absl::flat_hash_map<char, char> kEscapes = {{'n', '\n'},  {'t', '\t'},
                                                  {'\\', '\\'}, {'\'', '\''},
                                                  {'"', '"'},   {'{', '{'}};

// clang-format off
const absl::flat_hash_map<absl::string_view, TokenKind> kKeywords = {
    {"var", kVar},   {"if", kIf},
    {"else", kElse}, {"in", kIn},
    {"for", kFor},   {"func", kFunc},
};
// clang-format on

constexpr auto kTripleQuote = R"(""")";

// clang-format off
const absl::flat_hash_map<TokenKind, absl::string_view> kTokenMnemonics = {
  {kUnknown, "<UNKNOWN>"},{kError, "<ERROR>"}, {kBegin, "<BEGIN>"}, {kEnd, "<END>"},

  // literals
  {kIdent, "<IDENT>"},
  {kIntLit, "<INT_LIT>"},
  {kFloatLit, "<FLOAT_LIT>"},
  {kBoolLit, "<BOOL_LIT>"},

  // symbols
  {kOpenParen, "("},
  {kCloseParen, ")"},
  {kOpenBracket, "["},
  {kCloseBracket, "]"},
  {kOpenBrace, "{"},
  {kCloseBrace, "}"},
  {kOpenChev, "<"},
  {kCloseChev, ">"},

  {kEq, "="},
  {kBang, "!"},
  {kEqEq, "=="},
  {kBangEq, "!="},
  {kGtEq, ">="},
  {kLtEq, "<="},
  {kPlus, "+"},
  {kMinus, "-"},
  {kStar, "*"},
  {kSlash, "/"},
  {kPercent, "%"},

  {kSingleQuote, "'"},
  {kDoubleQuote, "\""},
  {kSemi, ";"},
  {kComma, ","},
  {kDot, "."},

  // template stuff
  {kTemplateBegin, "<TEMPLATE_BEGIN>"},
  {kTemplateEnd, "<TEMPLATE_END>"},
  {kTemplateSegment, "<TEMPLATE_SEGMENT>"},
  {kTemplateExprBegin, "<TEMPLATE_EXPR_BEGIN>"},
  {kTemplateExprEnd, "<TEMPLATE_EXPR_END>"},

  // keywords
  {kVar, "var"},
  {kIf, "if"},
  {kElse, "else"},
  {kIn, "in"},
  {kFor, "for"},
  {kFunc, "func"},

  {kReserved, "<RESERVED>"}
};
// clang-format on

enum class IntBase { kDec, kHex, kBin };
}  // namespace

TextSpan TextSpan::Merge(const TextSpan left, const TextSpan right) {
  const auto begin = std::min(left.offset, right.offset);
  const auto end = std::max(left.end(), right.end());
  return {begin, end - begin};
}

std::string_view GetTokenMnemonic(TokenKind kind) {
  CHECK(kTokenMnemonics.contains(kind));
  return kTokenMnemonics.at(kind);
}

bool Tokenizer::Next() {
  switch (GetState().kind) {
    case State::Kind::kNormal:
      return NextNormal();
    case State::Kind::kTemplate:
      return NextTemplate(false);
    case State::Kind::kTemplateMultiline:
      return NextTemplate(true);
    default:
      ABSL_UNREACHABLE();
  }
}

const Token& Tokenizer::current() const { return current_; }
bool Tokenizer::NextNormal() {
  bool init = false;
  while (!has_error_ && !AtEnd()) {
    init = true;

    BeginToken();

    // skip non-semantics tokens
    if (TryConsumeWhitespace() || TryConsumeComment()) {
      EndToken();
      continue;
    }

    // kIdent, keywords
    if (TryConsumeIdent()) {
      EndToken();
      return true;
    }

    // kIntLit
    if (TryConsumeInt()) {
      EndToken();
      return true;
    }

    // kTemplate*
    if (TryConsumeTemplateBegin()) {
      EndToken();
      return true;
    }

    // all symbols
    if (TryConsumeSymbol()) {
      EndToken();
      return true;
    }

    has_error_ = true;
    NextChar();
  }

  if (!init) {
    BeginToken();
  }

  // clear error and return error token
  if (has_error_) {
    current_.kind = TokenKind::kError;
    EndToken();
    has_error_ = false;
    return true;
  }

  current_.kind = TokenKind::kEnd;
  EndToken();
  return false;
}
bool Tokenizer::NextTemplate(bool multiline) {
  BeginToken();

  while (!has_error_ && !AtEnd()) {
    if (AtChar('{')) {
      NextChar();
      PushState(State::Kind::kNormal);
      current_.kind = TokenKind::kTemplateExprBegin;
      return true;
    }

    if (AtChar('\"') || At(kTripleQuote)) {
      NextChar();
      PopState();
      current_.kind = TokenKind::kTemplateEnd;
      return true;
    }

    if (TryConsumeTemplateSegment(multiline)) {
      current_.kind = TokenKind::kTemplateSegment;
      EndToken();
      return true;
    }

    has_error_ = true;
    NextChar();
  }

  if (has_error_) {
    current_.kind = TokenKind::kError;
    EndToken();
    has_error_ = false;
    return true;
  }

  current_.kind = TokenKind::kEnd;
  EndToken();
  return false;
}

bool Tokenizer::TryConsumeIdent() {
  if (!At(kIdentBegin)) {
    return false;
  }
  while (At(kIdentSegment) && !AtEnd()) {
    NextChar();
  }
  const auto text = source_.substr(record_start_, pos_ - record_start_);
  if (text == "true") {
    current_.kind = kBoolLit;
    current_.value_kind = ValueKind::kBool;
    current_.bool_v = true;
    return true;
  }
  if (text == "false") {
    current_.kind = kBoolLit;
    current_.value_kind = ValueKind::kBool;
    current_.bool_v = false;
    return true;
  }
  if (kKeywords.contains(text)) {
    current_.kind = kKeywords.at(text);
  } else {
    current_.kind = TokenKind::kIdent;
    current_.value_kind = ValueKind::kIdent;
    current_.str_v = text;
  }
  return true;
}
bool Tokenizer::TryConsumeInt() {
  bool neg = false;
  if (!At(kDigits)) {
    if (AtChar('-')) {
      neg = true;
      NextChar();
      if (!At(kDigits)) {
        return false;
      }
    } else {
      return false;
    }
  }

  auto base = IntBase::kDec;
  if (AtChar('0')) {
    NextChar();
    if (AtChar('x') || AtChar('X')) {
      base = IntBase::kHex;
      NextChar();
    } else if (AtChar('b') || AtChar('B')) {
      base = IntBase::kBin;
      NextChar();
    }
  }

  absl::CharSet digits;
  switch (base) {
    case IntBase::kDec:
      digits = kDigits;
      break;
    case IntBase::kHex:
      digits = kHexDigits;
      break;
    case IntBase::kBin:
      digits = kBinDigits;
      break;
  }

  uint64_t value = 0;
  while (At(digits) && !AtEnd()) {
    const auto c = PeekChar();
    NextChar();
    if (c == '_') continue;
    switch (base) {
      case IntBase::kDec: {
        value = value * 10L + (c - '0');
        break;
      }
      case IntBase::kHex: {
        uint64_t n;
        if (c > 'a' && c < 'f') {
          n = c - 'a' + 10;
        } else if (c > 'A' && c < 'F') {
          n = c - 'A' + 10;
        } else {
          n = c - '0';
        }
        value = value * 16L + n;
        break;
      }
      case IntBase::kBin: {
        value = value * 2L + (c - '0');
        break;
      }
    }
  }
  if (base == IntBase::kDec) {
    bool is_float = false;
    if (AtChar('.') && absl::CharSet::AsciiDigits().contains(PeekChar(1))) {
      is_float = true;
      NextChar();  // consume '.'
      while (At(kDigits) && !AtEnd()) {
        NextChar();
      }
    }
    if (AtChar('e') || AtChar('E')) {
      is_float = true;
      NextChar();  // consume 'e'/'E'
      if (AtChar('+') || AtChar('-')) {
        NextChar();
      }
      while (At(kDigits) && !AtEnd()) {
        NextChar();
      }
    }
    if (is_float) {
      scratch_buffer_.clear();
      for (const char c : source_.substr(record_start_, pos_ - record_start_)) {
        if (c != '_') {
          scratch_buffer_ += c;
        }
      }
      current_.float_v = std::stod(scratch_buffer_);
      current_.value_kind = ValueKind::kFloat;
      current_.kind = TokenKind::kFloatLit;
      return true;
    }
  }

  if (neg) {
    current_.int_v = -value;
    current_.value_kind = ValueKind::kSignedInt;
    current_.kind = TokenKind::kIntLit;
  } else {
    if (value > std::numeric_limits<int64_t>::max()) {
      current_.uint_v = value;
      current_.value_kind = ValueKind::kUnsignedInt;
    } else {
      current_.int_v = value;
      current_.value_kind = ValueKind::kSignedInt;
    }
    current_.kind = TokenKind::kIntLit;
  }
  return true;
}
bool Tokenizer::TryConsumeSymbol() {
  const char c1 = PeekChar();
  switch (c1) {
    case '{': {
      GetState().braces++;
      NextChar();
      current_.kind = TokenKind::kOpenBrace;
      return true;
    }
    case '}': {
      auto& state = GetState();
      if (--state.braces == 0) {
        PopState();
        current_.kind = TokenKind::kTemplateExprEnd;
      } else {
        current_.kind = TokenKind::kCloseBrace;
      }
      NextChar();
      return true;
    }
    case '[':
      NextChar();
      current_.kind = TokenKind::kOpenBracket;
      return true;
    case ']':
      NextChar();
      current_.kind = TokenKind::kCloseBracket;
      return true;
    case '(':
      NextChar();
      current_.kind = TokenKind::kOpenParen;
      return true;
    case ')':
      NextChar();
      current_.kind = TokenKind::kCloseParen;
      return true;
    case '<':
      NextChar();
      if (AtChar('=')) {
        NextChar();
        current_.kind = TokenKind::kLtEq;
        return true;
      }
      current_.kind = TokenKind::kOpenChev;
      return true;
    case '>':
      NextChar();
      if (AtChar('=')) {
        NextChar();
        current_.kind = TokenKind::kGtEq;
        return true;
      }
      current_.kind = TokenKind::kCloseChev;
      return true;
    case ';':
      NextChar();
      current_.kind = TokenKind::kSemi;
      return true;
    case ',':
      NextChar();
      current_.kind = TokenKind::kComma;
      return true;
    case '.':
      NextChar();
      current_.kind = TokenKind::kDot;
      return true;
    case '\'':
      NextChar();
      current_.kind = TokenKind::kSingleQuote;
      return true;
    case '\"':
      NextChar();
      current_.kind = TokenKind::kDoubleQuote;
      return true;
    case '!':
      NextChar();
      if (AtChar('=')) {
        NextChar();
        current_.kind = TokenKind::kBangEq;
        return true;
      }
      current_.kind = TokenKind::kBang;
      return true;
    case '-':
      NextChar();
      current_.kind = TokenKind::kMinus;
      return true;
    case '+':
      NextChar();
      current_.kind = TokenKind::kPlus;
      return true;
    case '*':
      NextChar();
      current_.kind = TokenKind::kStar;
      return true;
    case '/':
      NextChar();
      current_.kind = TokenKind::kSlash;
      return true;
    case '%':
      NextChar();
      current_.kind = TokenKind::kPercent;
      return true;
    case '=':
      NextChar();
      if (AtChar('=')) {
        NextChar();
        current_.kind = TokenKind::kEqEq;
        return true;
      }
      current_.kind = TokenKind::kEq;
      return true;
    default:
      return false;
  }
}
bool Tokenizer::TryConsumeWhitespace() {
  if (At(kNewLine)) {
    NextChar();
    return true;
  }

  if (At(kWhitespace)) {
    while (At(kWhitespace)) {
      NextChar();
    }
    return true;
  }

  return false;
}
bool Tokenizer::TryConsumeComment() {
  if (AtChar('#')) {
    NextChar();
    while (!At(kNewLine) && !AtEnd()) {
      NextChar();
    }
    return true;
  }
  return false;
}

bool Tokenizer::TryConsumeTemplateBegin() {
  if (At(kTripleQuote)) {
    NextChar();
    PushState(State::Kind::kTemplateMultiline);
    current_.kind = TokenKind::kTemplateBegin;
    return true;
  }
  if (AtChar('"')) {
    NextChar();
    PushState(State::Kind::kTemplate);
    current_.kind = TokenKind::kTemplateBegin;
    return true;
  }
  return false;
}
bool Tokenizer::TryConsumeTemplateSegment(const bool multiline) {
  scratch_buffer_.clear();
  while (true) {
    if (AtEnd()) {
      has_error_ = true;
      return false;
    }

    if (AtChar('{')) {
      break;
    }

    if (multiline) {
      if (At(kTripleQuote)) {
        break;
      }
    } else {
      if (AtChar('"')) {
        break;
      }
    }

    if (At(kNewLine)) {
      if (!multiline) {
        has_error_ = true;
        return false;
      }
      break;
    }

    if (PeekChar() == '\\') {
      NextChar();
      const auto c = PeekChar();
      if (c == 'u' || c == 'U') {
        // TODO: escape unicode
        ABSL_UNREACHABLE();
      }
      if (!kEscapes.contains(c)) {
        diagnostic_->Report(
            Diagnostic::Error("Illegal escape")
                .withLabel(TextSpan(pos_, 1),
                           absl::StrFormat("escape `%c` doesn't exists", c)));
        has_error_ = true;
        return false;
      }
      const auto escaped = kEscapes.at(c);
      scratch_buffer_ += escaped;
    } else {
      scratch_buffer_ += PeekChar();
    }

    NextChar();
  }

  current_.kind = TokenKind::kTemplateSegment;
  current_.str_v = zone_->InternString(scratch_buffer_);
  return true;
}

void Tokenizer::NextChar() {
  if (pos_ >= source_.size()) {
    return;
  }
  pos_++;
}
char Tokenizer::PeekChar(const int offset) const {
  if (pos_ + offset >= source_.size()) {
    return kEof;
  } else {
    return source_[pos_ + offset];
  }
}
bool Tokenizer::At(const absl::CharSet& chars) const {
  return chars.contains(PeekChar());
}
bool Tokenizer::At(const std::string_view str) const {
  for (int n = 0; n < str.length(); n++) {
    if (PeekChar(n) != str[n]) {
      return false;
    }
  }
  return true;
}
bool Tokenizer::AtChar(const char c) const { return PeekChar() == c; }
bool Tokenizer::AtEnd() const { return PeekChar() == kEof; }
bool Tokenizer::TryConsume(const char c) {
  if (AtChar(c)) {
    NextChar();
    return true;
  } else {
    return false;
  }
}
void Tokenizer::BeginToken() {
  current_.kind = TokenKind::kBegin;
  current_.span.offset = pos_;
  record_start_ = pos_;
}
void Tokenizer::EndToken() {
  current_.span.length = pos_ - current_.span.offset;
  record_start_ = 0;
}
Tokenizer::State& Tokenizer::GetState() {
  CHECK_GE(state_.size(), 0);
  return state_.top();
}
void Tokenizer::PushState(State::Kind kind) { state_.emplace(kind, 1); }
void Tokenizer::PopState() {
  CHECK_GE(state_.size(), 0);
  state_.pop();
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
