//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_CODE_OBJECT_H
#define WERSALKALANG_CODE_OBJECT_H

#include <span>

#include "runtime/vm/bytecode.h"
#include "runtime/vm/object.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

struct CodeObject {
  explicit CodeObject(const std::span<const Instr>& instructions,
                      const std::span<const ConstantDesc>& constants,
                      const uint32_t arg_count, const uint32_t max_stack,
                      const uint32_t max_locals)
      : instructions(instructions),
        constants(constants),
        arg_count(arg_count),
        max_stack(max_stack),
        max_locals(max_locals) {}

  std::span<const Instr> instructions;
  std::span<const ConstantDesc> constants;

  uint32_t arg_count;
  uint32_t max_stack;
  uint32_t max_locals;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_CODE_OBJECT_H
