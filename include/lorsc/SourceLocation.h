#pragma once

#include <cstdint>

namespace lorsc {

struct SourceLocation {
  std::int32_t line = 1;
  std::int32_t column = 1;
};

}  // namespace lorsc


