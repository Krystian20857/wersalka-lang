//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_SYM_H
#define WERSALKALANG_SYM_H
#include "zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class LocalsTable : public ZoneObject {
 public:
  explicit LocalsTable(Zone* zone, const int start_slot)
      : zone_(zone), locals_(zone), free_slots_(zone), next_slot_(start_slot) {}

  void PushScope();
  void PopScope();

  int DefineVar(std::string_view name);
  int DefineSyntheticVar();
  std::optional<int> LookupVar(std::string_view name) const;

  int max_locals() const { return max_locals_; }

 private:
  struct Local {
    std::string_view name;
    int slot;
    int depth;
  };

  Zone* zone_;
  ZoneList<Local> locals_;
  ZoneList<int> free_slots_;
  int scope_depth_ = 0;
  int next_slot_ = 0;
  int max_locals_ = 0;
  int synthetic_id_ = 0;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_SYM_H
