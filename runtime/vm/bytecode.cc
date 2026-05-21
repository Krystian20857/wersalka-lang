//
// Created by nothingbutyou on 5/16/26.
//

#include "bytecode.h"

#include "absl/strings/str_format.h"

namespace wersalka {
namespace lang {
namespace runtime {

namespace {
#define DEFINE_OPCODE(_mnemonic) \
  OpcodeInfo { .mnemonic = _mnemonic }

// clang-format off
constexpr auto kOpcodes =
    std::array<OpcodeInfo, static_cast<size_t>(Opcode::kReserved)>{
        DEFINE_OPCODE("NOP"),

        DEFINE_OPCODE("PUSH_CONST"),

        DEFINE_OPCODE("LOAD_LOCAL"),
        DEFINE_OPCODE("STORE_LOCAL"),
        DEFINE_OPCODE("LOAD_GLOBAL"),
        DEFINE_OPCODE("STORE_GLOBAL"),

        DEFINE_OPCODE("JMP"),
        DEFINE_OPCODE("JMP_IF_TRUE"),
        DEFINE_OPCODE("JMP_IF_FALSE"),

        DEFINE_OPCODE("POP"),
        DEFINE_OPCODE("DUP"),
        DEFINE_OPCODE("SWAP"),

        DEFINE_OPCODE("ADD"),
        DEFINE_OPCODE("SUB"),
        DEFINE_OPCODE("MUL"),
        DEFINE_OPCODE("DIV"),
        DEFINE_OPCODE("MOD"),
        DEFINE_OPCODE("AND"),
        DEFINE_OPCODE("OR"),
        DEFINE_OPCODE("XOR"),
        DEFINE_OPCODE("SHL"),
        DEFINE_OPCODE("SHR"),
        DEFINE_OPCODE("CMP_GT"),
        DEFINE_OPCODE("CMP_LT"),
        DEFINE_OPCODE("CMP_GE"),
        DEFINE_OPCODE("CMP_LE"),
        DEFINE_OPCODE("CMP_EQ"),
        DEFINE_OPCODE("NEG"),

        DEFINE_OPCODE("INVOKE"),
        DEFINE_OPCODE("RETURN"),
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
  return {.kind = Kind::kUInt, .str_v = str};
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
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
