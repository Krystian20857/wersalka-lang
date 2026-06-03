//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_DRIVER_H
#define WERSALKALANG_DRIVER_H

#include <string_view>
#include <vector>

namespace wersalka {
namespace lang {
namespace runtime {

class Driver {
 public:
  int RunFiles(std::vector<std::string_view> files);
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_DRIVER_H
