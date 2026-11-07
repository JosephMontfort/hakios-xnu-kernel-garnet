// The Adreno 662v2 chip facts, for the GPU of the SM7475 (Snapdragon 7+ Gen 2,
// POCO F5 "marble"): the same "a6xx gen4" register family as the A635 the
// driver was written for, but a different chip id, SMMU generation and GMU.
//
// IMPORTANT correction about this chip. The POCO F5's GPU is often called the
// "Adreno 725" (its marketing name), but the SM7475 device tree reports
//   qcom,chipid = 0x06060201,  qcom,gpu-model = "Adreno662v2" / "Adreno644v1"
// so to freedreno/Mesa this is the a6xx gen4 family (chip id 0x06060201), NOT
// a7xx. That makes this port far closer to the existing a6xx.hpp table than a
// from-scratch a7xx port: the a6xx register offsets mostly carry over, and the
// differences are the chip facts below plus the SM7475 memory map.
//
// SM7475 register bases (marble.dts qcom,kgsl-3d0 / kgsl-smmu / gpucc):
//   kgsl_3d0_reg_memory = 0x03d00000  (0x40000)   - GPU core / CP registers
//   cx_dbgc             = 0x03d61000  (0x800)     - CX debug
//   rscc                = 0x03de0000  (0x10000)   - RSC / GMU sequencer block
//   qdss_gfx            = 0x10900000  (0x80000)   - QDSS gfx
//   kgsl_smmu (qsmmu-v500) = 0x03da0000 (0x20000) - GPU SMMU (NOT the 0x5040000
//                                                 arm-smmu the A630 flow used)
//   gpucc               = 0x03d90000  (0xa000)    - GPU clock controller
// Firmware (vendor/firmware): a662_gmu.bin, a662_zap.{mdt,b00,b01,b02,mbn},
//   a660_sqe.fw. The zap is required for the CP to leave secure mode.
#pragma once

#include <cstdint>

namespace hakios::metal::a662 {

// --- this chip -----------------------------------------------------------------------------------------------------
// From marble.dts: qcom,chipid 0x06060201, "Adreno662v2" (vk-device-id
// 0x06060201 for gpu-model-id 0x5f / "Adreno644v1"; other speed bins report
// 0x06060202/0x06060203). Same a6xx gen4 register family as the A635.

inline constexpr uint32_t kChipId = 0x06060201;             // a662: Adreno662v2 on SM7475 (marble.dts qcom,chipid)
inline constexpr uint32_t kChipIdMask = 0xffffff00;         // match the family (low byte = speed bin / model variant)
inline constexpr uint32_t kChipIdFamily = 0x06060200;       // 0x060602xx covers the A644v1/A645v1/A649v1 bins
inline constexpr uint32_t kGpuId = 725;                     // marketing model ("Adreno 725"); sweet ABI GPU_ID

// Chip facts. Because this is the same gen4 family as the A635, the geometry
// values start from the a6xx numbers and are marked `verify:` where the A662
// may differ (freedreno has an a662 entry only in newer Mesa).
inline constexpr bool kHasLpac = true;                      // a6xx gen4 LPAC (verify: present on A662?)
inline constexpr bool kSupportsDoubleThreadsize = true;     // a6xx_base
inline constexpr uint32_t kInstrCacheSize = 127;            // preload cap (gen4 hang workaround, same as A635)
inline constexpr uint32_t kFibersPerSp = 128 * 2 * 16;      // verify: A662 fibers_per_sp
inline constexpr uint32_t kNumSpCores = 2;                  // verify: A662 has 2 SP (num_ccu)
inline constexpr uint32_t kCsSharedMemSize = 32 * 1024;     // verify: shared local store
inline constexpr uint32_t kThreadsizeBase = 64;             // a6xx default
inline constexpr uint32_t kMaxWaves = 16;                   // a6xx default max_waves
inline constexpr uint32_t kWaveGranularity = 2;             // a6xx gen4 granularity
inline constexpr uint32_t kRegidNone = 0xfc;                // ir3 INVALID_REG
inline constexpr uint32_t kGmemSize = 1024 * 1024;          // verify: A662 GMEM (1 MiB like A630?)
inline constexpr uint32_t kGmemBase = 0x100000;             // sweet ABI GMEM_BASE
inline constexpr uint32_t kMaxFreq = 710000000;             // a6xx reference cap; DCVS table from marble.dts pwrlevels

// 64-bit address-register alignments (gen4, same as a6xx). verify:
inline constexpr uint32_t kSpCsObjStartAlign = 32;
inline constexpr uint32_t kSpCsPvtMemAddrAlign = 32;
inline constexpr uint32_t kSpCsIboAlign = 16;
inline constexpr uint32_t kSpCsTexConstAlign = 64;
inline constexpr uint32_t kSpCsTexSampAlign = 16;
inline constexpr uint32_t kDelayLoopCycles = 16;

// --- SM7475 MMIO bases (marble.dts) -----------------------------------------------------------------------------------
// These replace the beryllium/A630 addresses baked into the kernel's
// SweetAdreno bring-up. A real port rewrites the driver to take them from the
// ADT; until then they are the constants a binary patch must substitute.
inline constexpr uint64_t kGpuCoreBase = 0x03d00000;        // kgsl_3d0_reg_memory (CP registers)
inline constexpr uint64_t kGpuCoreSize = 0x40000;
inline constexpr uint64_t kGpuCcBase = 0x03d90000;          // gpucc (clock controller)
inline constexpr uint64_t kGpuCcSize = 0xa000;
inline constexpr uint64_t kGpuSmmuBase = 0x03da0000;        // kgsl_smmu (qsmmu-v500)
inline constexpr uint64_t kGpuSmmuSize = 0x20000;
inline constexpr uint64_t kGpuRsccBase = 0x03de0000;        // rscc / RSC sequencer
inline constexpr uint64_t kGpuRsccSize = 0x10000;
inline constexpr uint64_t kGpuCxDbgcBase = 0x03d61000;      // cx_dbgc
inline constexpr uint64_t kGpuQdssBase = 0x10900000;        // qdss_gfx
inline constexpr uint64_t kGpuQdssSize = 0x80000;
// interrupts: kgsl_3d0_irq SPI 0x12c, freq_limiter_irq SPI 0x11e
inline constexpr uint32_t kGpuIrqSpi = 0x12c;
inline constexpr uint32_t kGpuFreqLimiterIrqSpi = 0x11e;

// Whether the a662 register table is confirmed for real submissions. Same gen4
// family as the A635 so the a6xx table is the starting point; flip this once
// the few A662-specific offsets are verified against a downstream kgsl header.
inline constexpr bool kA662RegistersConfirmed = false;

}  // namespace hakios::metal::a662
