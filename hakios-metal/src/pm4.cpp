#include "hakios-metal/pm4.hpp"

#include <atomic>
#include <cassert>

namespace hakios::metal::pm4 {

Header decode(uint32_t word) {
    Header h;
    switch (word >> 28) {
    case 4:
        h.count = word & 0x7f;
        h.reg = (word >> 8) & 0x3ffff;
        if (((word >> 7) & 1) == oddParity(h.count) && ((word >> 27) & 1) == oddParity(h.reg)) {
            h.kind = Header::kRegisters;
        }
        break;
    case 7:
        h.count = word & 0x3fff;
        h.opcode = (word >> 16) & 0x7f;
        if (((word >> 15) & 1) == oddParity(h.count) && ((word >> 23) & 1) == oddParity(h.opcode)) {
            h.kind = Header::kCommand;
        }
        break;
    default:
        break;
    }
    return h;
}

void Stream::command(uint32_t opcode, uint32_t count) {
    assert(count <= kMaxType7Count);
    words_.push_back(type7(opcode, count));
}

namespace {
std::atomic<bool> gA7xx{false};

struct Move {
    uint32_t a6xx, count, a7xx;   // COUNT registers from A6XX on are at A7XX on; a7xx kDropRegister: not on the A7xx
};

// Mesa 25.0.7 src/freedreno/registers/adreno/a6xx.xml, the registers hakios-metal writes that are variants="A6XX"
constexpr Move kMoves[] = {
    {0x9980, 1, 0x9107},   // PC_RASTER_CNTL
    {0x9981, 1, 0x9809},   // PC_POLYGON_MODE
    {0xacc0, 1, 0xa9bf},   // SP_2D_DST_FORMAT
    {0xb4c0, 13, 0xb2c0},  // SP_PS_2D_SRC_INFO .. SP_PS_2D_SRC_FLAGS_PITCH (0xb4c0-0xb4cc)
    {0xb4d1, 1, 0xab21},   // SP_WINDOW_OFFSET
    {0xb800, 1, 0xa827},   // HLSQ_VS_CNTL (SP_VS_CONST_CONFIG)
    {0xb801, 1, 0xa83f},   // HLSQ_HS_CNTL
    {0xb802, 1, 0xa867},   // HLSQ_DS_CNTL
    {0xb803, 1, 0xa898},   // HLSQ_GS_CNTL
    {0xb980, 1, 0xa9c6},   // HLSQ_FS_CNTL_0
    {0xb981, 1, kDropRegister},   // HLSQ_UNKNOWN_B981
    {0xb982, 5, 0xa9c7},   // HLSQ_CONTROL_1_REG .. HLSQ_CONTROL_5_REG
    {0xb987, 1, 0xa9cd},   // HLSQ_CS_CNTL
    {0xb990, 7, 0xa9d4},   // HLSQ_CS_NDRANGE_0 .. 6
    {0xb997, 1, kDropRegister},   // HLSQ_CS_CNTL_0: the A7xx has SP_CS_CNTL_0 alone (compute.cpp writes it)
    {0xb998, 1, kDropRegister},   // HLSQ_CS_CNTL_1: moved to 0xa9db with another layout (compute.cpp writes it)
    {0xb999, 3, 0xa9dc},   // HLSQ_CS_KERNEL_GROUP_X .. Z
    {0xb9d0, 1, kDropRegister},   // HLSQ_CS_CTRL_REG1
    {0xbb08, 1, 0xab1f},   // HLSQ_INVALIDATE_CMD
    {0xbb10, 1, 0xab03},   // HLSQ_FS_CNTL (SP_PS_CONST_CONFIG)
    {0xbb11, 1, kDropRegister},   // HLSQ_SHARED_CONSTS
    {0xbe00, 2, kDropRegister},   // HLSQ_UNKNOWN_BE00, BE01
    {0xbe04, 1, kDropRegister},   // HLSQ_DBG_ECO_CNTL (the A710's magic table has its own)
    {0xb605, 1, kDropRegister},   // TPL1_UNKNOWN_B605
    {0x8a00, 1, kDropRegister},   // RB_UNKNOWN_8A00
    {0x8a10, 1, kDropRegister},   // RB_UNKNOWN_8A10
    {0x8a20, 1, kDropRegister},   // RB_UNKNOWN_8A20
    {0x8a30, 1, kDropRegister},   // RB_UNKNOWN_8A30
    {0x9210, 2, kDropRegister},   // VPC_UNKNOWN_9210, 9211
    {0x9805, 1, kDropRegister},   // PC_POWER_CNTL: turnip writes it on A6XX only (tu_emit_bin_preamble)
    {0xa0f8, 1, kDropRegister},   // VFD_POWER_CNTL: likewise
};
}  // namespace

uint32_t a7xxRegister(uint32_t reg) {
    for (const Move& m : kMoves) {
        if (reg >= m.a6xx && reg < m.a6xx + m.count) {
            return m.a7xx == kDropRegister ? kDropRegister : m.a7xx + (reg - m.a6xx);
        }
    }
    return reg;
}

void setA7xx(bool on) { gA7xx.store(on); }
bool isA7xx() { return gA7xx.load(std::memory_order_relaxed); }

void Stream::registers(uint32_t reg, const uint32_t* values, uint32_t count) {
    if (isA7xx()) {
        // translated register by register, the runs that stay consecutive kept as one packet
        uint32_t i = 0;
        while (i < count) {
            const uint32_t to = a7xxRegister(reg + i);
            if (to == kDropRegister) {
                i++;
                continue;
            }
            uint32_t n = 1;
            while (i + n < count && a7xxRegister(reg + i + n) == to + n) n++;
            put(to, values + i, n);
            i += n;
        }
        return;
    }
    put(reg, values, count);
}

void Stream::put(uint32_t reg, const uint32_t* values, uint32_t count) {
    // more than a header can count goes as several packets, each starting where the one before stopped
    while (count > 0) {
        uint32_t n = count < kMaxType4Count ? count : kMaxType4Count;
        words_.push_back(type4(reg, n));
        words_.insert(words_.end(), values, values + n);
        reg += n;
        values += n;
        count -= n;
    }
}

void Stream::nop(uint32_t words) {
    command(kNop, words);
    words_.insert(words_.end(), words, 0u);
}

void Stream::waitForIdle() { command(kWaitForIdle, 0); }
void Stream::waitMemWrites() { command(kWaitMemWrites, 0); }
void Stream::waitForMe() { command(kWaitForMe, 0); }

void Stream::memWrite(uint64_t a, const uint32_t* values, uint32_t count) {
    while (count > 0) {
        uint32_t n = count < kMaxType7Count - 2 ? count : kMaxType7Count - 2;
        command(kMemWrite, 2 + n);
        address(a);
        words_.insert(words_.end(), values, values + n);
        a += 4ull * n;
        values += n;
        count -= n;
    }
}

void Stream::memcpy(uint64_t destination, uint64_t source, uint32_t words) {
    command(kMemcpy, 5);
    words_.push_back(words);
    address(source);
    address(destination);
}

void Stream::indirect(uint64_t a, uint32_t words) {
    command(kIndirectBuffer, 3);
    address(a);
    words_.push_back(words);
}

void Stream::packet(uint32_t opcode, const uint32_t* words, uint32_t count) {
    command(opcode, count);
    words_.insert(words_.end(), words, words + count);
}

}  // namespace hakios::metal::pm4
