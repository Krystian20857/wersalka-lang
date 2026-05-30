//
// Created by nothingbutyou on 5/16/26.
//

#ifndef WERSALKALANG_BYTECODE_H
#define WERSALKALANG_BYTECODE_H

#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class BytecodeBuilder;

// clang-format off
enum class Opcode : uint8_t {
  kNop,

  kPushConst,

  kLoadLocal, kStoreLocal,
  kLoadGlobal, kStoreGlobal,

  // TODO: could I have only `kJmpIfTrue`?
  kJmp, kJmpIfTrue, kJmpIfFalse,

  kPop, kDup, kSwap,

  kAdd, kSub, kMul, kDiv, kMod,
  kAnd, kOr, kXor, kShl, kShr,
  kCmpGt, kCmpLt, kCmpGe, kCmpLe, kCmpEq,
  kNeg,

  kInvoke, kReturn,

  kNewArray, kStoreArray, kLoadArray,

  kReserved
};
// clang-format on

struct OpcodeInfo {
  std::string_view mnemonic;
  int stack_in;
  int stack_out;
};

const OpcodeInfo* GetOpcodeInfo(Opcode opcode);

// 64-bit fixed-length instruction
struct Instr {
  Opcode op;
  uint16_t c1;
  uint32_t c2;
};

struct ConstantDesc {
  static ConstantDesc CreateUInt(uint64_t value);
  static ConstantDesc CreateNull();
  static ConstantDesc CreateString(std::string_view str);

  enum class Kind { kInt, kUInt, kFloat, kBool, kNull, kString };
  Kind kind;
  union {
    int64_t int_v;
    uint64_t uint_v;
    double float_v;
    bool bool_v;
    std::string_view str_v;
  };
};

class Label {
 public:
  Label() : Label(-1) {}

 private:
  friend class BytecodeBuilder;

  explicit Label(const int id) : id_(id) {}

  int id_;
};

class BytecodeBuilder {
 public:
  explicit BytecodeBuilder(Zone* zone)
      : zone_(zone),
        instructions_(zone, 128),
        labels_(zone, 32),
        constants_(zone, 32) {}

  Label NewLabel();
  void BindLabel(Label label);

  int EmitPushConst(ConstantDesc constant);
  int Emit(Opcode opcode, uint16_t c1, uint32_t c2);
  int EmitVarGlobal(Opcode opcode, ZoneStr symbol_name);
  int EmitVarLocal(Opcode opcode, uint16_t slot);
  int EmitInvoke(Opcode opcode, uint16_t arg_count);
  int Emit(Opcode opcode);
  void EmitJump(Opcode opcode, Label label);
  void EmitJump(Opcode opcode, uint16_t c1, Label label);

  // more like next bci, idc
  int current_bci() const { return instructions_.size(); }

  const ZoneList<Instr>& instructions() const { return instructions_; }
  const ZoneList<ConstantDesc>& constants() const { return constants_; }

 private:
  struct LabelState {
    explicit LabelState(Zone* zone) : uses(zone) {}

    bool bound = false;
    int pos = 0;
    ZoneList<int> uses;
  };

  static constexpr auto kUnpatchedJump = 0xFFFF;

  int AddConstant(ConstantDesc desc);

  Zone* zone_;
  ZoneList<Instr> instructions_;
  ZoneList<LabelState> labels_;

  // TODO: introduce constant pool here
  ZoneList<ConstantDesc> constants_;
};

class BytecodeDisassembler {
 public:
  BytecodeDisassembler(const std::span<const Instr>& instructions,
                       const std::span<const ConstantDesc>& constants)
      : instructions_(instructions), constants_(constants) {}

  void Disassemble(std::ostream& stream) const;
  std::string FormatConstant(const ConstantDesc& constant) const;

 private:
  std::span<const Instr> instructions_;
  std::span<const ConstantDesc> constants_;
};

int ComputeMaxStackDepth(std::span<const Instr> instructions,
                      std::span<const ConstantDesc> constants);

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_BYTECODE_H
