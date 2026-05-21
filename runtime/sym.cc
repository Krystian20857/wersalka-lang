//
// Created by nothingbutyou on 5/17/26.
//

#include "sym.h"

#include "absl/strings/str_format.h"

namespace wersalka::lang::runtime {

void LocalsTable::PushScope() { scope_depth_++; }
void LocalsTable::PopScope() {
  while (!locals_.empty()) {
    const auto& local = locals_.Back();
    if (local.depth != scope_depth_) {
      break;
    }
    free_slots_.Add(zone_, local.slot);
    locals_.PopBack();
  }
  scope_depth_--;
}
int LocalsTable::DefineVar(const std::string_view name) {
  // disallow duplicated var in the same scope
  for (const auto& local : locals_) {
    if (local.depth == scope_depth_ && local.name == name) {
      return local.slot;
    }
  }

  int slot;
  if (!free_slots_.empty()) {
    slot = free_slots_.Back();
    free_slots_.PopBack();
  } else {
    slot = next_slot_++;
  }

  const auto local = Local{.name = name, .slot = slot, .depth = scope_depth_};
  locals_.Add(zone_, local);

  if (slot + 1 > max_locals_) {
    max_locals_ = slot + 1;
  }
  return slot;
}
int LocalsTable::DefineSyntheticVar() {
  return DefineVar(
      zone_->InternString(absl::StrFormat("__tmp_%d", ++synthetic_id_)));
}
std::optional<int> LocalsTable::LookupVar(const std::string_view name) const {
  for (const auto& local : locals_) {
    if (local.name == name) {
      return local.slot;
    }
  }
  return std::nullopt;
}
}  // namespace wersalka::lang::runtime
