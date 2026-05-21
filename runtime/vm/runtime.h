//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_RUNTIME_H
#define WERSALKALANG_RUNTIME_H

#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class Runtime {
 public:
  explicit Runtime(Zone* zone) : zone_(zone) {}

  Zone* GetPermanentZone() const { return zone_; }

 private:
  Zone* zone_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_RUNTIME_H
