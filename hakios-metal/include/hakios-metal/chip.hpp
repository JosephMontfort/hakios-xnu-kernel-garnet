// Which Adreno this driver is running on, resolved once from the kernel's
// GET_PARAM chip id, and the per-generation facts the emitters need without
// knowing which family is underneath.
//
// The register tables stay per-generation (a6xx.hpp for A635 / A662; a7xx.hpp for A710);
// this header is the single place that picks the chip facts by id.
#pragma once

#include <cstdint>

#include "hakios-metal/a6xx.hpp"
#include "hakios-metal/a662.hpp"
#include "hakios-metal/a7xx.hpp"

namespace hakios::metal {

enum class ChipGeneration : uint8_t {
    kUnknown = 0,
    kA6xx = 6,     // a6xx gen4: A635 (SM7325) chip 0x06030500, A662 (SM7475) chip 0x060602xx
    kA7xx = 7,     // a7xx gen1: A710 (SM7435-AB, garnet) chip 0x07010000
};

// The generation a kernel-reported chip id belongs to. Match family bits (upper words).
inline constexpr ChipGeneration generationOf(uint32_t chipId) {
    if ((chipId & a7xx::kChipIdMask) == a7xx::kChipIdFamily) {
        return ChipGeneration::kA7xx;   // Adreno 710 (SM7435-AB)
    }
    if ((chipId & 0xffff0000u) == (a6xx::kChipId & 0xffff0000u)) {
        return ChipGeneration::kA6xx;   // A635 (SM7325)
    }
    if ((chipId & a662::kChipIdMask) == a662::kChipIdFamily) {
        return ChipGeneration::kA6xx;   // A662 (SM7475)
    }
    return ChipGeneration::kUnknown;
}

// A friendlier chip label for logging.
inline constexpr const char* chipName(uint32_t chipId) {
    if ((chipId & a7xx::kChipIdMask) == a7xx::kChipIdFamily) {
        return "a710 (Adreno 710, SM7435-AB, garnet)";
    }
    if ((chipId & a662::kChipIdMask) == a662::kChipIdFamily) {
        return "a662 (Adreno 725 / Adreno662v2, SM7475, marble)";
    }
    if ((chipId & 0xffff0000u) == (a6xx::kChipId & 0xffff0000u)) {
        return "a635 (Adreno 7c+ Gen 3 / A635, SM7325, redwood)";
    }
    return "unknown";
}

inline constexpr const char* generationName(ChipGeneration g) {
    switch (g) {
    case ChipGeneration::kA7xx: return "a7xx gen1";
    case ChipGeneration::kA6xx: return "a6xx gen4";
    default: return "unknown";
    }
}

// Whether a command stream may actually be submitted to this chip.
// A635 (reference) and A710 (verified on garnet) are usable.
inline constexpr bool usable(uint32_t chipId) {
    if ((chipId & a7xx::kChipIdMask) == a7xx::kChipIdFamily) {
        return true;   // Adreno 710 target
    }
    if ((chipId & 0xffff0000u) == (a6xx::kChipId & 0xffff0000u)) {
        return true;   // A635 target
    }
    if ((chipId & a662::kChipIdMask) == a662::kChipIdFamily) {
        return a662::kA662RegistersConfirmed;
    }
    return false;
}

// The per-generation facts an emitter needs, one shape for all supported chips.
struct ChipInfo {
    ChipGeneration generation = ChipGeneration::kUnknown;
    uint32_t chipId = 0;              // Raw id the kernel reported
    const char* name = "unknown";
    uint32_t numSpCores = 0;
    uint32_t fibersPerSp = 0;
    uint32_t csSharedMemSize = 0;
    uint32_t threadsizeBase = 0;
    uint32_t maxWaves = 0;
    uint32_t waveGranularity = 0;
    uint32_t instrCacheSize = 0;
    uint32_t gmemSize = 0;
    bool hasLpac = false;
    bool doubleThreadsize = false;
    uint32_t highestBankBit = 14;
    uint32_t numCcu = 2;
    uint32_t ubwcMode = 2;
    uint32_t tileAlignW = 32;
    uint32_t tileAlignH = 16;
    uint32_t numVscPipes = 16;
};

inline constexpr ChipInfo infoFor(uint32_t chipId) {
    if ((chipId & a7xx::kChipIdMask) == a7xx::kChipIdFamily) {
        return ChipInfo{
            ChipGeneration::kA7xx, chipId, "a710/Adreno710",
            a7xx::kNumSpCores, a7xx::kFibersPerSp, a7xx::kCsSharedMemSize,
            a7xx::kThreadsizeBase, a7xx::kMaxWaves, a7xx::kWaveGranularity,
            a7xx::kInstrCacheSize, a7xx::kGmemSize,
            a7xx::kHasLpac, a7xx::kSupportsDoubleThreadsize,
            a7xx::kHighestBankBit, a7xx::kNumCcu, a7xx::kUbwcMode,
            a7xx::kTileAlignW, a7xx::kTileAlignH, a7xx::kNumVscPipes
        };
    }
    if ((chipId & a662::kChipIdMask) == a662::kChipIdFamily) {
        return ChipInfo{
            ChipGeneration::kA6xx, chipId, "a662/Adreno725",
            a662::kNumSpCores, a662::kFibersPerSp, a662::kCsSharedMemSize,
            a662::kThreadsizeBase, a662::kMaxWaves, a662::kWaveGranularity,
            a662::kInstrCacheSize, a662::kGmemSize,
            a662::kHasLpac, a662::kSupportsDoubleThreadsize,
            14, 2, 3, 32, 16, 16
        };
    }
    if ((chipId & 0xffff0000u) == (a6xx::kChipId & 0xffff0000u)) {
        return ChipInfo{
            ChipGeneration::kA6xx, chipId, "a635",
            a6xx::kNumSpCores, a6xx::kFibersPerSp, a6xx::kCsSharedMemSize,
            a6xx::kThreadsizeBase, a6xx::kMaxWaves, a6xx::kWaveGranularity,
            a6xx::kInstrCacheSize, 512 * 1024,
            a6xx::kHasLpac, a6xx::kSupportsDoubleThreadsize,
            14, 2, 2, 32, 16, 16
        };
    }
    return ChipInfo{};
}

}  // namespace hakios::metal
