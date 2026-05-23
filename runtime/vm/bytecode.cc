//
// Created by nothingbutyou on 5/16/26.
//

#include "bytecode.h"

#include "absl/strings/str_format.h"

namespace wersalka {
namespace lang {
namespace runtime {

namespace {
constexpr auto kExtraStackSize = 4;

#define DEFINE_OPCODE(_mnemonic, _stack_in, _stack_out)                   \
  OpcodeInfo {                                                            \
    .mnemonic = _mnemonic, .stack_in = _stack_in, .stack_out = _stack_out \
  }

// clang-format off
constexpr auto kOpcodes =
    std::array<OpcodeInfo, static_cast<size_t>(Opcode::kReserved)>{
        DEFINE_OPCODE("NOP",           0, 0),

        DEFINE_OPCODE("PUSH_CONST",    0, 1),

        DEFINE_OPCODE("LOAD_LOCAL",    0, 1),
        DEFINE_OPCODE("STORE_LOCAL",   1, 0),
        DEFINE_OPCODE("LOAD_GLOBAL",   0, 1),
        DEFINE_OPCODE("STORE_GLOBAL",  1, 0),

        DEFINE_OPCODE("JMP",           0, 0),
        DEFINE_OPCODE("JMP_IF_TRUE",   1, 0),
        DEFINE_OPCODE("JMP_IF_FALSE",  1, 0),

        DEFINE_OPCODE("POP",           1, 0),
        DEFINE_OPCODE("DUP",           1, 2),
        DEFINE_OPCODE("SWAP",          1, 1),

        DEFINE_OPCODE("ADD",           2, 1),
        DEFINE_OPCODE("SUB",           2, 1),
        DEFINE_OPCODE("MUL",           2, 1),
        DEFINE_OPCODE("DIV",           2, 1),
        DEFINE_OPCODE("MOD",           2, 1),
        DEFINE_OPCODE("AND",           2, 1),
        DEFINE_OPCODE("OR",            2, 1),
        DEFINE_OPCODE("XOR",           2, 1),
        DEFINE_OPCODE("SHL",           2, 1),
        DEFINE_OPCODE("SHR",           2, 1),
        DEFINE_OPCODE("CMP_GT",        2, 1),
        DEFINE_OPCODE("CMP_LT",        2, 1),
        DEFINE_OPCODE("CMP_GE",        2, 1),
        DEFINE_OPCODE("CMP_LE",        2, 1),
        DEFINE_OPCODE("CMP_EQ",        2, 1),
        DEFINE_OPCODE("NEG",           1, 1),

        DEFINE_OPCODE("INVOKE",        1, 1),
        DEFINE_OPCODE("RETURN",        1, 0),
    };
// clang-format on

#undef DEFINE_OPCODE
}  // namespace

const OpcodeInfo* GetOpcodeInfo(Opcode opcode) {
  return &kOpcodes[static_cast<size_t>(opcode)];
}
ConstantDesc ConstantDesc::CreateUInt(const uint64_t value) {
  return {.kind = Kind::kUInt, .uint_v = value};
}
ConstantDesc ConstantDesc::CreateNull() { return {.kind = Kind::kNull}; }
ConstantDesc ConstantDesc::CreateString(const std::string_view str) {
  // ReSharper disable once CppDFALocalValueEscapesFunction
  return {.kind = Kind::kString, .str_v = str};
}
Label BytecodeBuilder::NewLabel() {
  const auto id = labels_.size();
  labels_.Add(zone_, LabelState{zone_});
  return Label{id};
}
void BytecodeBuilder::BindLabel(const Label label) {
  auto& label_state = labels_[label.id_];
  CHECK(!label_state.bound);

  label_state.bound = true;
  label_state.pos = current_bci();

  for (int use_bci : label_state.uses) {
    CHECK(use_bci < instructions_.size());
    instructions_[use_bci].c2 = label_state.pos;  // c2 is jmp target
  }

  label_state.uses.Clear();
}
int BytecodeBuilder::EmitPushConst(ConstantDesc constant) {
  const auto constant_idx = AddConstant(constant);
  return Emit(Opcode::kPushConst, 0, constant_idx);
}
int BytecodeBuilder::Emit(const Opcode opcode, const uint16_t c1,
                          const uint32_t c2) {
  const auto bci = current_bci();
  instructions_.Add(zone_, Instr{opcode, c1, c2});
  return bci;
}
int BytecodeBuilder::EmitVarGlobal(Opcode opcode, ZoneStr symbol_name) {
  CHECK(opcode == Opcode::kLoadGlobal || opcode == Opcode::kStoreGlobal);
  const auto constant_idx =
      AddConstant(ConstantDesc{.kind = ConstantDesc::Kind::kString,
                               .str_v = zone_->InternString(symbol_name)});
  return Emit(opcode, 0, constant_idx);
}
int BytecodeBuilder::EmitVarLocal(Opcode opcode, uint16_t slot) {
  CHECK(opcode == Opcode::kLoadLocal || opcode == Opcode::kStoreLocal);
  return Emit(opcode, slot, 0);
}
int BytecodeBuilder::EmitInvoke(Opcode opcode, uint16_t arg_count) {
  CHECK(opcode == Opcode::kInvoke);
  return Emit(opcode, arg_count, 0);
}
int BytecodeBuilder::Emit(Opcode opcode) { return Emit(opcode, 0, 0); }
void BytecodeBuilder::EmitJump(Opcode opcode, Label label) {
  EmitJump(opcode, 0, label);
}
void BytecodeBuilder::EmitJump(const Opcode opcode, const uint16_t c1,
                               const Label label) {
  CHECK(opcode == Opcode::kJmp || opcode == Opcode::kJmpIfTrue ||
        opcode == Opcode::kJmpIfFalse);
  auto& label_state = labels_[label.id_];
  if (label_state.bound) {
    Emit(opcode, c1, label_state.pos);
    return;
  }
  const auto bci = Emit(opcode, c1, kUnpatchedJump);
  label_state.uses.Add(zone_, bci);
}
int BytecodeBuilder::AddConstant(ConstantDesc desc) {
  const auto id = constants_.size();
  constants_.Add(zone_, desc);
  return id;
}
void BytecodeDisassembler::Disassemble(std::ostream& stream) const {
  for (auto bci = 0; bci < instructions_.size(); ++bci) {
    const auto instr = instructions_[bci];
    const auto info = GetOpcodeInfo(instr.op);
    stream << absl::StrFormat("\t%-6d %s", bci, info->mnemonic);
    switch (instr.op) {
      case Opcode::kPushConst: {
        stream << " "
               << absl::StrFormat("`%s`", FormatConstant(constants_[instr.c2]));
        break;
      }
      case Opcode::kLoadLocal:
      case Opcode::kStoreLocal: {
        stream << " " << absl::StrFormat("`%d`", instr.c1);
        break;
      }
      case Opcode::kLoadGlobal:
      case Opcode::kStoreGlobal: {
        stream << " "
               << absl::StrFormat("`%s`", FormatConstant(constants_[instr.c2]));
        break;
      }
      case Opcode::kJmp:
      case Opcode::kJmpIfTrue:
      case Opcode::kJmpIfFalse: {
        stream << " " << absl::StrFormat("bci: `%d`", instr.c2);
        break;
      }
      case Opcode::kInvoke: {
        stream << " " << absl::StrFormat("args: `%d`", instr.c1);
        break;
      }
      default: {
        // noop
        break;
      }
    }
    stream << "\n";
  }
}
std::string BytecodeDisassembler::FormatConstant(
    const ConstantDesc& constant) const {
  switch (constant.kind) {
    case ConstantDesc::Kind::kInt:
      return std::to_string(constant.int_v);
    case ConstantDesc::Kind::kUInt:
      return std::to_string(constant.uint_v);
    case ConstantDesc::Kind::kFloat:
      return std::to_string(constant.float_v);
    case ConstantDesc::Kind::kBool:
      return std::to_string(constant.bool_v);
    case ConstantDesc::Kind::kNull:
      return "null";
    case ConstantDesc::Kind::kString:
      return std::string{constant.str_v};
    default:
      ABSL_UNREACHABLE();
  }
}
int ComputeMaxStackDepth(std::span<const Instr> instructions,
                         std::span<const ConstantDesc> constants) {
  if (instructions.empty()) {
    return kExtraStackSize;
  }

  std::vector<int> depth(instructions.size(), -1);
  std::vector<int> worklist;  // bci list

  depth[0] = 0;
  worklist.push_back(0);

  int max_depth = 0;

  while (!worklist.empty()) {
    const auto bci = worklist.back();
    worklist.pop_back();

    const auto instr = instructions[bci];
    const auto opcode_info = GetOpcodeInfo(instr.op);
    const auto stack_effect =
        instr.op == Opcode::kInvoke
            ? -instr.c1
            : (opcode_info->stack_out - opcode_info->stack_in);
    const auto new_stack = depth[bci] + stack_effect;
    max_depth = std::max(max_depth, new_stack);

    const auto propagate = [&](int bci) {
      if (depth[bci] == -1) {
        depth[bci] = new_stack;
        worklist.push_back(bci);
      } else {
        CHECK(depth[bci] == new_stack)
            << "stack mismatch on frame merge, bci " << bci << ", expected "
            << new_stack << " was " << depth[bci];
      }
    };

    switch (instr.op) {
      case Opcode::kJmp: {
        propagate(instr.c2);
        break;
      }
      case Opcode::kJmpIfFalse:
      case Opcode::kJmpIfTrue: {
        propagate(instr.c2);
        propagate(bci + 1);
        break;
      }
      default: {
        if (bci + 1 < instructions.size()) {
          propagate(bci + 1);
        }
      }
    }
  }

  return max_depth;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
