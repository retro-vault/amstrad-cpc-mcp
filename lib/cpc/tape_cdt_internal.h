//
// CDT decoder details shared by the control-flow and sampled-data units.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef CPC_TAPE_CDT_INTERNAL_H
#define CPC_TAPE_CDT_INTERNAL_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "cpc/types.h"
#include "tape_internal.h"

namespace cpc::tape_internal {

struct cdt_block {
    u8 id = 0;
    std::size_t file_offset = 0;
    std::vector<u8> body;
};

inline u16 cdt_u16(std::span<const u8> data, std::size_t offset)
{
    return static_cast<u16>(data[offset] | (data[offset + 1] << 8));
}

inline u32 cdt_u24(std::span<const u8> data, std::size_t offset)
{
    return static_cast<u32>(data[offset] | (data[offset + 1] << 8) |
                            (data[offset + 2] << 16));
}

inline u32 cdt_u32(std::span<const u8> data, std::size_t offset)
{
    return static_cast<u32>(data[offset]) |
           (static_cast<u32>(data[offset + 1]) << 8) |
           (static_cast<u32>(data[offset + 2]) << 16) |
           (static_cast<u32>(data[offset + 3]) << 24);
}

bool append_cdt_csw(builder &out, const cdt_block &block,
                    std::string &error);
bool append_cdt_generalized(builder &out, const cdt_block &block,
                            std::string &error);

} // namespace cpc::tape_internal

#endif // CPC_TAPE_CDT_INTERNAL_H
