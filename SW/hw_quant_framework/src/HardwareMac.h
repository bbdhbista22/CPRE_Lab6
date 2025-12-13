#pragma once

#include <cstddef>
#include <cstdint>

namespace ML {


class HardwareMac {
   public:
    static int32_t run(const uint16_t* packed_pairs, std::size_t pair_count);
};

}  // namespace ML
