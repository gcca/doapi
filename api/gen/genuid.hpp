#pragma once

#include <cstdint>

namespace genuid {
void InitParameters();
[[nodiscard]] std::uint64_t GenerateUID();
} // namespace genuid
