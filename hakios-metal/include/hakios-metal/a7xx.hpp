// The Adreno 710 chip facts, registers, and PM4 definitions for the GPU of the
// SM7435-AB (Snapdragon 7s Gen 2, Xiaomi Redmi Note 13 Pro 5G "garnet").
//
// Every hardware parameter and register here is verified against:
//   1. Garnet device sysfs and DT (/sys/class/kgsl/kgsl-3d0/device/of_node/)
//   2. Downstream Xiaomi kernel headers (adreno-gpulist.h, gen7_reg.h, adreno_gen7.c)
//   3. Mesa upstream freedreno device tables (tools/mesa/adreno-710.patch, freedreno_devices.py)
//   4. Mesa register XMLs (a6xx.xml, a7xx_enums.xml, adreno_pm4.xml)
#pragma once

#include <cstdint>

namespace hakios::metal::a7xx {

// --- Hardware Identification & Chip Facts ----------------------------------------------------------------------------
// DeviceTree compatible: "qcom,adreno-gpu-gen7-3-0"
// Sysfs gpu_model: "Adreno710v1"
// Kernel adreno-gpulist.h:2148: .chipid = 0x07010000
inline constexpr uint32_t kChipId = 0x07010000;             // Adreno 710 on SM7435 (Gen7 Gen 1)
inline constexpr uint32_t kChipIdMask = 0xffff0000;         // Match family/major/minor
inline constexpr uint32_t kChipIdFamily = 0x07010000;       // 0x0701xxxx covers all speed bins
inline constexpr uint32_t kGpuId = 710;                     // Marketing model ("Adreno 710")

// Chip geometry facts (adreno-gpulist.h:2156-2170, freedreno_devices.py A6xxGPUInfo for FD710)
inline constexpr uint32_t kGmemSize = 512 * 1024;           // 512 KiB GMEM (SZ_512K)
inline constexpr uint32_t kHighestBankBit = 15;             // highest_bank_bit = 15
inline constexpr uint32_t kNumCcu = 1;                     // 1 CCU (vs 2 on A635/A660, 3 on A730)
inline constexpr uint32_t kNumSpCores = 1;                 // 1 SP core cluster
inline constexpr uint32_t kTileAlignW = 64;                // 64-pixel tile width alignment
inline constexpr uint32_t kTileAlignH = 32;                // 32-pixel tile height alignment
inline constexpr uint32_t kNumVscPipes = 32;               // 32 VSC pipes for binning
inline constexpr uint32_t kCsSharedMemSize = 32 * 1024;    // 32 KiB local memory (CS shared memory)
inline constexpr uint32_t kWaveGranularity = 2;            // wave granularity 2
inline constexpr uint32_t kFibersPerSp = 128 * 2 * 16;     // 4096 fibers per SP
inline constexpr bool kHasLpac = false;                    // No LPAC on A710 (adreno_gen7.c:535/896)
inline constexpr bool kSupportsDoubleThreadsize = false;   // A7xx single threadsize
inline constexpr uint32_t kInstrCacheSize = 127;           // Preload cap (same 127 units limit)
inline constexpr uint32_t kThreadsizeBase = 64;            // 64 thread default
inline constexpr uint32_t kMaxWaves = 16;                  // 16 max waves
inline constexpr uint32_t kUbwcMode = 4;                   // UBWC 4.0 (DT qcom,ubwc-mode = 0x04000000)
inline constexpr uint32_t kBusWidth = 32;                  // 32 bytes bus width

// --- SM7435 MMIO Bases (DeviceTree /sys/class/kgsl/kgsl-3d0/device/of_node/reg) -------------------------------------
inline constexpr uint64_t kGpuCoreBase = 0x03d00000;        // kgsl_3d0_reg_memory (CP / 3D core registers)
inline constexpr uint64_t kGpuCoreSize = 0x40000;          // 256 KiB
inline constexpr uint64_t kGpuCxDbgcBase = 0x03d61000;      // cx_dbgc
inline constexpr uint64_t kGpuCxDbgcSize = 0x800;
inline constexpr uint64_t kGpuRsccBase = 0x03d50000;        // rscc (SM7435 is 0x03d50000 vs SM7475 0x03de0000)
inline constexpr uint64_t kGpuRsccSize = 0x10000;
inline constexpr uint64_t kGpuCxMiscBase = 0x03d9e000;      // cx_misc
inline constexpr uint64_t kGpuCxMiscSize = 0x1000;
inline constexpr uint64_t kGpuQdssBase = 0x10900000;        // qdss_gfx
inline constexpr uint64_t kGpuQdssSize = 0x80000;

// --- A7xx Register Offsets Differing from A6xx (Mesa a6xx.xml & gen7_reg.h) ----------------------------------------
// Window offset registers
inline constexpr uint32_t REG_A7XX_SP_WINDOW_OFFSET = 0xab21;        // a6xx.xml:4581 (vs A6xx 0xb4d1)
inline constexpr uint32_t REG_A7XX_TPL1_A2D_WINDOW_OFFSET = 0xb2d1;  // a6xx.xml:4572 (vs A6xx 0xb307)

// Stream-out control
inline constexpr uint32_t REG_A7XX_VPC_SO_CNTL = 0x9305;             // a6xx.xml:3522 (vs A6xx 0x9216)

// Shader constant configuration registers
inline constexpr uint32_t REG_A7XX_SP_VS_CONST_CONFIG = 0xa827;      // a6xx.xml:4632 (vs A6xx 0xb800)
inline constexpr uint32_t REG_A7XX_SP_HS_CONST_CONFIG = 0xa83f;      // a6xx.xml:4633 (vs A6xx 0xb801)
inline constexpr uint32_t REG_A7XX_SP_DS_CONST_CONFIG = 0xa867;      // a6xx.xml:4634 (vs A6xx 0xb802)
inline constexpr uint32_t REG_A7XX_SP_GS_CONST_CONFIG = 0xa898;      // a6xx.xml:4635 (vs A6xx 0xb803)
inline constexpr uint32_t REG_A7XX_SP_CS_CONST_CONFIG = 0xa9cd;      // a6xx.xml:4731 (vs A6xx 0xb987)
inline constexpr uint32_t REG_A7XX_SP_PS_CONST_CONFIG = 0xab03;      // a6xx.xml:4966 (vs A6xx 0xbb10)

// A7xx ECO / Debug Control registers
inline constexpr uint32_t REG_A7XX_TPL1_DBG_ECO_CNTL = 0xb600;       // a6xx.xml:4585
inline constexpr uint32_t REG_A7XX_TPL1_DBG_ECO_CNTL1 = 0xb602;      // a6xx.xml:4591
inline constexpr uint32_t REG_A7XX_GRAS_DBG_ECO_CNTL = 0x8600;       // a6xx.xml:2400
inline constexpr uint32_t REG_A7XX_SP_CHICKEN_BITS = 0xae03;         // a6xx.xml:4748
inline constexpr uint32_t REG_A7XX_UCHE_CLIENT_PF = 0x0e19;          // a6xx.xml:390
inline constexpr uint32_t REG_A7XX_PC_MODE_CNTL = 0x9804;            // a6xx.xml:3600
inline constexpr uint32_t REG_A7XX_SP_DBG_ECO_CNTL = 0xae00;         // a6xx.xml:4745
inline constexpr uint32_t REG_A7XX_RB_DBG_ECO_CNTL = 0x8e04;         // a6xx.xml:2750
inline constexpr uint32_t REG_A7XX_RB_UNKNOWN_8E01 = 0x8e01;
inline constexpr uint32_t REG_A7XX_VPC_DBG_ECO_CNTL = 0x9600;        // a6xx.xml:3580
inline constexpr uint32_t REG_A7XX_UCHE_UNKNOWN_0E12 = 0x0e12;       // a6xx.xml:385
inline constexpr uint32_t REG_A7XX_RB_UNKNOWN_8E06 = 0x8e06;
inline constexpr uint32_t REG_A7XX_UCHE_CACHE_WAYS = 0x0e17;         // a6xx.xml:388
inline constexpr uint32_t REG_A7XX_PC_DBG_ECO_CNTL = 0x9e00;         // a6xx.xml:3650
inline constexpr uint32_t REG_A7XX_PC_UNKNOWN_9E24 = 0x9e24;         // a6xx.xml:3655

// --- A7xx PM4 Event Types (adreno_pm4.xml:141-155, gen7_reg.h:18-23) ------------------------------------------------
enum a7xx_vgt_event_type : uint32_t {
    A7XX_DUMMY_EVENT = 0x01,
    A7XX_CCU_INVALIDATE_DEPTH = 0x18,      // 24
    A7XX_CCU_INVALIDATE_COLOR = 0x19,      // 25
    A7XX_CCU_RESOLVE_CLEAN = 0x1a,         // 26
    A7XX_CCU_FLUSH_DEPTH = 0x1c,           // 28
    A7XX_CCU_FLUSH_COLOR = 0x1d,           // 29
    A7XX_CCU_END_RESOLVE_GROUP = 0x1f,     // 31
    A7XX_CCU_CLEAN_DEPTH = 0x20,           // 32 (GEN7_INT_CCU_CLEAN_DEPTH_TS = 10)
    A7XX_CCU_CLEAN_COLOR = 0x21,           // 33 (GEN7_INT_CCU_CLEAN_COLOR_TS = 11)
    A7XX_CACHE_RESET = 0x30,               // 48
    A7XX_CACHE_CLEAN = 0x31,               // 49
    A7XX_CACHE_FLUSH7 = 0x32,              // 50 (A7xx cache flush with timestamp, vs A6xx 4)
    A7XX_CACHE_INVALIDATE7 = 0x33,         // 51 (A7xx cache invalidate, vs A6xx 49)
    A7XX_SUBPASS_FENCE = 0x35,             // 53
};
// CP_EVENT_WRITE7_0.WRITE_ENABLED (adreno_pm4.xml): the 4-word event writes its payload only with this bit
inline constexpr uint32_t A7XX_EVENT_WRITE_ENABLED = 1u << 27;


// --- Proven Adreno 710 Magic Register Values (tools/mesa/adreno-710.patch:8-58) -------------------------------------
struct RegVal {
    uint32_t reg;
    uint32_t value;
};

inline constexpr RegVal kA710MagicRegs[] = {
    {REG_A7XX_TPL1_DBG_ECO_CNTL, 0x01000000},
    {REG_A7XX_GRAS_DBG_ECO_CNTL, 0x00000800},
    {REG_A7XX_SP_CHICKEN_BITS, 0x00000400},
    {REG_A7XX_UCHE_CLIENT_PF, 0x00000084},
    {REG_A7XX_PC_MODE_CNTL, 0x00001f1f},
    {REG_A7XX_SP_DBG_ECO_CNTL, 0x10000000},
    {REG_A7XX_RB_DBG_ECO_CNTL, 0x00000000},
    {REG_A7XX_RB_UNKNOWN_8E01, 0x00000000},
    {REG_A7XX_VPC_DBG_ECO_CNTL, 0x02000000},
    {REG_A7XX_UCHE_UNKNOWN_0E12, 0x03200000},
    {REG_A7XX_RB_UNKNOWN_8E06, 0x00080000},
};

inline constexpr RegVal kA710RawMagicRegs[] = {
    {0x0e17, 0x00040004},  // UCHE_CACHE_WAYS
    {0xb602, 0x00000700},  // TPL1_DBG_ECO_CNTL1
    {0xae08, 0x00400400},  // SP_UNKNOWN_AE08
    {0xae09, 0x00000000},  // SP_UNKNOWN_AE09
    {0xae0a, 0x00000000},  // SP_UNKNOWN_AE0A
    {0x0e10, 0x00000000},  // UCHE_UNKNOWN_0E10
    {0x0e11, 0x00000000},  // UCHE_UNKNOWN_0E11
    {0xae6c, 0x00000000},  // SP_UNKNOWN_AE6C
    {0x9e00, 0x20100000},  // PC_DBG_ECO_CNTL
    {0x9e24, 0x01fc7f00},  // PC_UNKNOWN_9E24
    {0xa600, 0x00000000},  // VFD_UNKNOWN_A600
    {0xae06, 0x00000000},  // SP_UNKNOWN_AE06
    {0xae6a, 0x00000000},  // SP_UNKNOWN_AE6A
    {0xae6b, 0x00000080},  // SP_UNKNOWN_AE6B
    {0xae73, 0x00000000},  // SP_UNKNOWN_AE73
    {0xab01, 0x00000001},  // SP_UNKNOWN_AB01
    {0xab22, 0x00000000},  // SP_UNKNOWN_AB22
    {0xb310, 0x00000000},  // SP_UNKNOWN_B310
    {0x0ce2, 0x00000000},  // SP_UNKNOWN_0CE2
    {0x0ce3, 0x00000000},  // SP_UNKNOWN_0CE2 + 1
    {0x0ce4, 0x00000000},  // SP_UNKNOWN_0CE4
    {0x0ce5, 0x00000000},  // SP_UNKNOWN_0CE4 + 1
    {0x0ce6, 0x00000000},  // SP_UNKNOWN_0CE6
    {0x0ce7, 0x00000000},  // SP_UNKNOWN_0CE6 + 1
    {0x8008, 0x00000000},  // GRAS_UNKNOWN_8008
    {0x8e79, 0x00000000},  // RB_UNKNOWN_8E79
    {0x8899, 0x00000000},  // RB_UNKNOWN_8899
    {0x88f5, 0x00000000},  // RB_UNKNOWN_88F5
};

}  // namespace hakios::metal::a7xx
