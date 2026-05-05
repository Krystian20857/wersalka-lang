//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/tokenizer.h"

#include "gtest/gtest.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

std::vector<TokenKind> kindsOf(const std::vector<Token>& tokens) {
  std::vector<TokenKind> kinds;
  for (auto token : tokens) {
    kinds.push_back(token.kind);
  }
  return kinds;
}

// initializer_list -> vector fails inside EXPECT_EQ
std::vector<TokenKind> kindsOf(const std::initializer_list<TokenKind>& tokens) {
  return tokens;
}

struct TokenizerTestFixture : ::testing::Test {
  std::vector<Token> Tokenize(const std::string& source) {
    Tokenizer tokenizer(&zone, &reporter, source);
    std::vector<Token> tokens;
    while (true) {
      tokenizer.Next();
      const auto& current = tokenizer.current();
      if (current.kind == TokenKind::kBegin) {
        continue;
      }
      if (current.kind == TokenKind::kEnd) {
        break;
      }
      tokens.push_back(current);
    }
    return tokens;
  }

  Zone zone;
  DiagnosticReporter reporter;
};

struct TokenizerCase {
  std::string source;
  std::vector<TokenKind> token_kinds;
};

struct TokenizerParamTest : TokenizerTestFixture,
                            ::testing::WithParamInterface<TokenizerCase> {};

TEST_P(TokenizerParamTest, ProduceExpectedTokenKinds) {
  const auto& param = GetParam();
  const auto tokens = Tokenize(param.source);
  EXPECT_FALSE(reporter.HasError());
  EXPECT_EQ(kindsOf(tokens), param.token_kinds);
}

using enum TokenKind;

// clang-format off
INSTANTIATE_TEST_SUITE_P(BasicTokens, TokenizerParamTest,
  ::testing::Values(
    TokenizerCase {
      "var a = (2 + 2) * 2;",
      {
        kVar, kIdent, kEq,
        kOpenParen, kIntLit, kPlus, kIntLit, kCloseParen,
        kStar, kIntLit, kSemi
      }
    },
    TokenizerCase {
      "true false",
      {kBoolLit, kBoolLit}
    },
    TokenizerCase {
      "( ) [ ] { } < >",
      {
        kOpenParen, kCloseParen, kOpenBracket, kCloseBracket,
        kOpenBrace, kCloseBrace, kOpenChev, kCloseChev
      }
    },
    TokenizerCase {
      "= ! == != >= <= + - * / %",
      {
        kEq, kBang, kEqEq, kBangEq, kGtEq, kLtEq,
        kPlus, kMinus, kStar, kSlash, kPercent
      }
    },
    TokenizerCase {
      R"( "a = { 2 + 2 }" )",
      {
        kTemplateBegin, kTemplateSegment,
        kTemplateExprBegin,
          kIntLit, kPlus, kIntLit,
        kTemplateExprEnd,
        kTemplateEnd
      }
    },
    TokenizerCase {
      R"( "a = { "a" + "{ 2 + 2 }" }" )",
      {
        kTemplateBegin, kTemplateSegment,
        kTemplateExprBegin,
          kTemplateBegin, kTemplateSegment, kTemplateEnd,
          kPlus,
          kTemplateBegin,
          kTemplateExprBegin,
            kIntLit, kPlus, kIntLit,
          kTemplateExprEnd,
          kTemplateEnd,
        kTemplateExprEnd,
        kTemplateEnd
      }
    }
  )
);
// clang-format on

TEST_F(TokenizerTestFixture, CorrectEscapes) {
  const auto tokens = Tokenize(R"( "\n \\ \t \' \" \{" )");
  EXPECT_FALSE(reporter.HasError());
  EXPECT_EQ(kindsOf(tokens),
            kindsOf({kTemplateBegin, kTemplateSegment, kTemplateEnd}));
  EXPECT_EQ(tokens[1].str_v, "\n \\ \t ' \" {");
}

TEST_F(TokenizerTestFixture, InvalidEscapes) {
  const auto tokens = Tokenize(R"( "\d \; \\\\\\" )");
  EXPECT_TRUE(reporter.HasError());
}

TEST_F(TokenizerTestFixture, IntLiteral) {
  {
    const auto tokens = Tokenize("1000");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kIntLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kSignedInt);
    EXPECT_EQ(tokens[0].int_v, 1000);
  }

  {
    const auto tokens = Tokenize("-1000");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kIntLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kSignedInt);
    EXPECT_EQ(tokens[0].int_v, -1000);
  }

  {
    // should overflow to unsigned int
    const auto tokens = Tokenize("9_223_372_036_854_775_808");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kIntLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kUnsignedInt);
    EXPECT_EQ(tokens[0].int_v, 9223372036854775808ULL);
  }
}

TEST_F(TokenizerTestFixture, FloatLiteral) {
  {
    const auto tokens = Tokenize("3.14");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kFloatLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kFloat);
    EXPECT_DOUBLE_EQ(tokens[0].float_v, 3.14);
  }

  {
    const auto tokens = Tokenize("-3.14");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kFloatLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kFloat);
    EXPECT_DOUBLE_EQ(tokens[0].float_v, -3.14);
  }

  {
    // exponent without fractional part
    const auto tokens = Tokenize("1e10");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kFloatLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kFloat);
    EXPECT_DOUBLE_EQ(tokens[0].float_v, 1e10);
  }

  {
    // exponent with sign and fractional part
    const auto tokens = Tokenize("1.5e-3");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kFloatLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kFloat);
    EXPECT_DOUBLE_EQ(tokens[0].float_v, 1.5e-3);
  }

  {
    // underscore separators
    const auto tokens = Tokenize("1_000.5_5");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kFloatLit}));
    EXPECT_EQ(tokens[0].value_kind, ValueKind::kFloat);
    EXPECT_DOUBLE_EQ(tokens[0].float_v, 1000.55);
  }

  {
    // dot without following digit is not a float
    const auto tokens = Tokenize("1.");
    EXPECT_EQ(kindsOf(tokens), kindsOf({kIntLit, kDot}));
  }
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
