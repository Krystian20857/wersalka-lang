//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/parser.h"

#include "gtest/gtest.h"
#include "runtime/ast.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

struct ParserTestFixture : ::testing::Test {
  std::string Parse(std::string_view source) {
    Tokenizer tokenizer(&zone, &reporter, source);
    Parser parser(&zone, &tokenizer, &reporter);
    return DumpAST(parser.Parse(true));
  }

  std::string ParseUnit(std::string_view source) {
    Tokenizer tokenizer(&zone, &reporter, source);
    Parser parser(&zone, &tokenizer, &reporter);
    return DumpAST(parser.Parse());
  }

  Zone zone;
  DiagnosticReporter reporter;
};

struct ParserCase {
  std::string_view source;
  std::string_view expected;
};

struct ParserParamTest : ParserTestFixture,
                         ::testing::WithParamInterface<ParserCase> {};

TEST_P(ParserParamTest, ProducesExpectedAST) {
  const auto& param = GetParam();
  EXPECT_FALSE(reporter.HasError());
  EXPECT_EQ(Parse(param.source), param.expected);
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(Literals, ParserParamTest,
  ::testing::Values(
    ParserCase{"1",     "\nConstExpr [1]\n"},
    ParserCase{"3.14",  "\nConstExpr [3.14]\n"},
    ParserCase{"true",  "\nConstExpr [true]\n"},
    ParserCase{"false", "\nConstExpr [false]\n"}
  )
);

INSTANTIATE_TEST_SUITE_P(Arithmetic, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "1 + 2",
      R"(
BinaryExpr [+]
  ConstExpr [1]
  ConstExpr [2]
)"
    },
    ParserCase{
      "1 + 2 * 3",
      R"(
BinaryExpr [+]
  ConstExpr [1]
  BinaryExpr [*]
    ConstExpr [2]
    ConstExpr [3]
)"
    },
    ParserCase{
      "1 * 2 + 3",
      R"(
BinaryExpr [+]
  BinaryExpr [*]
    ConstExpr [1]
    ConstExpr [2]
  ConstExpr [3]
)"
    },
    ParserCase{
      "(1 + 2) * 3",
      R"(
BinaryExpr [*]
  GroupExpr
    BinaryExpr [+]
      ConstExpr [1]
      ConstExpr [2]
  ConstExpr [3]
)"
    },
    ParserCase{
  "(-1 + +2 + -(1)) * ~3 * !true",
  R"(
BinaryExpr [*]
  GroupExpr
    BinaryExpr [+]
      ConstExpr [-1]
      UnaryExpr [+]
        BinaryExpr [+]
          ConstExpr [2]
          UnaryExpr [-]
            GroupExpr
              ConstExpr [1]
  UnaryExpr [~]
    BinaryExpr [*]
      ConstExpr [3]
      UnaryExpr [!]
        ConstExpr [true]
)"
}
  )
);

INSTANTIATE_TEST_SUITE_P(Associativity, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "1 - 2 - 3",
      R"(
BinaryExpr [-]
  BinaryExpr [-]
    ConstExpr [1]
    ConstExpr [2]
  ConstExpr [3]
)"
    },
    ParserCase{
      "2 ** 3 ** 4",
      R"(
BinaryExpr [**]
  ConstExpr [2]
  BinaryExpr [**]
    ConstExpr [3]
    ConstExpr [4]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Bitwise, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "1 & 2 | 3",
      R"(
BinaryExpr [|]
  BinaryExpr [&]
    ConstExpr [1]
    ConstExpr [2]
  ConstExpr [3]
)"
    },
    ParserCase{
      "1 << 2 + 3",
      R"(
BinaryExpr [<<]
  ConstExpr [1]
  BinaryExpr [+]
    ConstExpr [2]
    ConstExpr [3]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Comparison, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "1 == 2",
      R"(
BinaryExpr [==]
  ConstExpr [1]
  ConstExpr [2]
)"
    },
    ParserCase{
      "1 < 2",
      R"(
BinaryExpr [<]
  ConstExpr [1]
  ConstExpr [2]
)"
    }
  )
);
INSTANTIATE_TEST_SUITE_P(Ident, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "foo",
      "\nIdentExpr [foo]\n"
    },
    ParserCase{
      "foo + bar",
      R"(
BinaryExpr [+]
  IdentExpr [foo]
  IdentExpr [bar]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Assign, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "x = 1",
      R"(
AssignExpr [=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x += 1",
      R"(
AssignExpr [+=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x -= 1",
      R"(
AssignExpr [-=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x *= 2",
      R"(
AssignExpr [*=]
  IdentExpr [x]
  ConstExpr [2]
)"
    },
    ParserCase{
      "x /= 2",
      R"(
AssignExpr [/=]
  IdentExpr [x]
  ConstExpr [2]
)"
    },
    ParserCase{
      "x %= 2",
      R"(
AssignExpr [%=]
  IdentExpr [x]
  ConstExpr [2]
)"
    },
    ParserCase{
      "x &= 1",
      R"(
AssignExpr [&=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x |= 1",
      R"(
AssignExpr [|=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x ^= 1",
      R"(
AssignExpr [^=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x <<= 1",
      R"(
AssignExpr [<<=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "x >>= 1",
      R"(
AssignExpr [>>=]
  IdentExpr [x]
  ConstExpr [1]
)"
    },
    ParserCase{
      "a = b = 1",
      R"(
AssignExpr [=]
  IdentExpr [a]
  AssignExpr [=]
    IdentExpr [b]
    ConstExpr [1]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Call, ParserParamTest,
  ::testing::Values(
    ParserCase{
      "foo()",
      R"(
CallExpr
  IdentExpr [foo]
)"
    },
    ParserCase{
      "foo(1)",
      R"(
CallExpr
  IdentExpr [foo]
  ConstExpr [1]
)"
    },
    ParserCase{
      "foo(1, 2, 3)",
      R"(
CallExpr
  IdentExpr [foo]
  ConstExpr [1]
  ConstExpr [2]
  ConstExpr [3]
)"
    },
    ParserCase{
      "foo()()",
      R"(
CallExpr
  CallExpr
    IdentExpr [foo]
)"
    },
    ParserCase{
      "foo(1 + 2, bar)",
      R"(
CallExpr
  IdentExpr [foo]
  BinaryExpr [+]
    ConstExpr [1]
    ConstExpr [2]
  IdentExpr [bar]
)"
    }
  )
);
// clang-format on

struct CompileUnitParamTest : ParserTestFixture,
                              ::testing::WithParamInterface<ParserCase> {};

TEST_P(CompileUnitParamTest, ProducesExpectedAST) {
  const auto& param = GetParam();
  EXPECT_FALSE(reporter.HasError());
  EXPECT_EQ(ParseUnit(param.source), param.expected);
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(ExprStmt, CompileUnitParamTest,
  ::testing::Values(
    ParserCase{
      "foo();",
      R"(
CompileUnit
  ExprStmt
    CallExpr
      IdentExpr [foo]
)"
    },
    ParserCase{
      "foo(); bar();",
      R"(
CompileUnit
  ExprStmt
    CallExpr
      IdentExpr [foo]
  ExprStmt
    CallExpr
      IdentExpr [bar]
)"
    },
    ParserCase{
      "x = 1;",
      R"(
CompileUnit
  ExprStmt
    AssignExpr [=]
      IdentExpr [x]
      ConstExpr [1]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Statements, CompileUnitParamTest,
  ::testing::Values(
    ParserCase{
      "",
      "\nCompileUnit\n"
    },
    ParserCase{
      "var x",
      R"(
CompileUnit
  VarStmt [x]
)"
    },
    ParserCase{
      "var x = 1",
      R"(
CompileUnit
  VarStmt [x]
    ConstExpr [1]
)"
    },
    ParserCase{
      "if true {}",
      R"(
CompileUnit
  IfStmt
    ConstExpr [true]
    BlockStmt
)"
    },
    ParserCase{
      "if true {} else if false {}",
      R"(
CompileUnit
  IfStmt
    ConstExpr [true]
    BlockStmt
    IfStmt
      ConstExpr [false]
      BlockStmt
)"
    },
    ParserCase{
      "while true {}",
      R"(
CompileUnit
  WhileStmt
    ConstExpr [true]
    BlockStmt
)"
    },
    ParserCase{
      "if true {var x = 1}",
      R"(
CompileUnit
  IfStmt
    ConstExpr [true]
    BlockStmt
      VarStmt [x]
        ConstExpr [1]
)"
    }
  )
);

INSTANTIATE_TEST_SUITE_P(Functions, CompileUnitParamTest,
  ::testing::Values(
    ParserCase{
      "func foo() {}",
      R"(
CompileUnit
  FunctionDecl [foo]
    BlockStmt
)"
    },
    ParserCase{
      "func foo(x y) {}",
      R"(
CompileUnit
  FunctionDecl [foo]
    Param [x]
    Param [y]
    BlockStmt
)"
    },
    ParserCase{
      "func foo() {var x = 1}",
      R"(
CompileUnit
  FunctionDecl [foo]
    BlockStmt
      VarStmt [x]
        ConstExpr [1]
)"
    },
    ParserCase{
      "func foo() {} var x = 1",
      R"(
CompileUnit
  FunctionDecl [foo]
    BlockStmt
  VarStmt [x]
    ConstExpr [1]
)"
    }
  )
);
// clang-format on

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
