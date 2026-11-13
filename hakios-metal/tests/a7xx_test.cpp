// Tests for Adreno 710 (SM7435-AB, garnet) chip facts, register offsets,
// magic ECO initialization values, and PM4 packet encoding.
//
// Every expected value here is grounded in verified hardware facts:
//   - Garnet DT (/sys/class/kgsl/kgsl-3d0/device/of_node/)
//   - Downstream Xiaomi kernel (adreno-gpulist.h, gen7_reg.h, adreno_gen7.c)
//   - Upstream Mesa (freedreno_devices.py, adreno-710.patch, a6xx.xml, adreno_pm4.xml)
#include "hakios-metal/a7xx.hpp"
#include "hakios-metal/chip.hpp"
#include "hakios-metal/pm4.hpp"
#include "test.hpp"

using namespace hakios::metal;
using namespace hakios::metal::a7xx;
namespace pm4 = hakios::metal::pm4;

TEST(a7xx_chip_identification_and_dispatch) {
    // Adreno 710 on SM7435 (garnet): chipid 0x07010000
    CHECK_EQ(uint32_t(generationOf(0x07010000)), uint32_t(ChipGeneration::kA7xx));
    CHECK_EQ(uint32_t(generationOf(0x07010001)), uint32_t(ChipGeneration::kA7xx));
    CHECK(usable(0x07010000));
    CHECK(std::string(chipName(0x07010000)).find("a710") != std::string::npos);

    // Verify other known chips still dispatch correctly
    CHECK_EQ(uint32_t(generationOf(0x06030500)), uint32_t(ChipGeneration::kA6xx));  // A635
    CHECK_EQ(uint32_t(generationOf(0x06060201)), uint32_t(ChipGeneration::kA6xx));  // A662
}

TEST(a7xx_hardware_geometry_facts) {
    ChipInfo info = infoFor(0x07010000);
    CHECK_EQ(uint32_t(info.generation), uint32_t(ChipGeneration::kA7xx));
    CHECK_EQ(info.chipId, 0x07010000u);

    // Verified facts from adreno-gpulist.h:2144-2172 and DT
    CHECK_EQ(info.gmemSize, 512u * 1024u);           // SZ_512K
    CHECK_EQ(info.highestBankBit, 15u);             // highest_bank_bit = 15
    CHECK_EQ(info.numCcu, 1u);                     // Single CCU on A710
    CHECK_EQ(info.numSpCores, 1u);
    CHECK_EQ(info.tileAlignW, 64u);                // 64x32 tile alignment
    CHECK_EQ(info.tileAlignH, 32u);
    CHECK_EQ(info.numVscPipes, 32u);               // 32 VSC binning pipes
    CHECK_EQ(info.csSharedMemSize, 32u * 1024u);   // 32 KiB local memory
    CHECK_EQ(info.waveGranularity, 2u);
    CHECK_EQ(info.fibersPerSp, 128u * 2u * 16u);
    CHECK(!info.hasLpac);                          // No LPAC on A710
    CHECK(!info.doubleThreadsize);
    CHECK_EQ(info.ubwcMode, 4u);                   // UBWC 4.0
}

TEST(a7xx_sm7435_mmio_bases) {
    // Verified from Garnet DT /sys/class/kgsl/kgsl-3d0/device/of_node/reg
    CHECK_EQ(kGpuCoreBase, 0x03d00000ull);
    CHECK_EQ(kGpuCoreSize, 0x40000ull);
    CHECK_EQ(kGpuCxDbgcBase, 0x03d61000ull);
    CHECK_EQ(kGpuCxDbgcSize, 0x800ull);
    CHECK_EQ(kGpuRsccBase, 0x03d50000ull);         // SM7435 is 0x3d50000 (not 0x3de0000)
    CHECK_EQ(kGpuRsccSize, 0x10000ull);
    CHECK_EQ(kGpuCxMiscBase, 0x03d9e000ull);
    CHECK_EQ(kGpuCxMiscSize, 0x1000ull);
    CHECK_EQ(kGpuQdssBase, 0x10900000ull);
    CHECK_EQ(kGpuQdssSize, 0x80000ull);
}

TEST(a7xx_register_offsets) {
    // Verified from Mesa a6xx.xml variants="A7XX" and gen7_reg.h
    CHECK_EQ(REG_A7XX_SP_WINDOW_OFFSET, 0xab21u);        // vs A6xx 0xb4d1
    CHECK_EQ(REG_A7XX_TPL1_A2D_WINDOW_OFFSET, 0xb2d1u);  // vs A6xx 0xb307
    CHECK_EQ(REG_A7XX_VPC_SO_CNTL, 0x9305u);             // vs A6xx 0x9216
    CHECK_EQ(REG_A7XX_SP_VS_CONST_CONFIG, 0xa827u);      // vs A6xx 0xb800
    CHECK_EQ(REG_A7XX_SP_HS_CONST_CONFIG, 0xa83fu);      // vs A6xx 0xb801
    CHECK_EQ(REG_A7XX_SP_DS_CONST_CONFIG, 0xa867u);      // vs A6xx 0xb802
    CHECK_EQ(REG_A7XX_SP_GS_CONST_CONFIG, 0xa898u);      // vs A6xx 0xb803
    CHECK_EQ(REG_A7XX_SP_CS_CONST_CONFIG, 0xa9cdu);      // vs A6xx 0xb987
    CHECK_EQ(REG_A7XX_SP_PS_CONST_CONFIG, 0xab03u);      // vs A6xx 0xbb10
}

TEST(a710_magic_eco_registers) {
    // Verified from tools/mesa/adreno-710.patch:8-58
    constexpr size_t kNumMagic = sizeof(kA710MagicRegs) / sizeof(kA710MagicRegs[0]);
    CHECK_EQ(kNumMagic, 11u);

    CHECK_EQ(kA710MagicRegs[0].reg, REG_A7XX_TPL1_DBG_ECO_CNTL);
    CHECK_EQ(kA710MagicRegs[0].value, 0x01000000u);

    CHECK_EQ(kA710MagicRegs[1].reg, REG_A7XX_GRAS_DBG_ECO_CNTL);
    CHECK_EQ(kA710MagicRegs[1].value, 0x00000800u);

    CHECK_EQ(kA710MagicRegs[2].reg, REG_A7XX_SP_CHICKEN_BITS);
    CHECK_EQ(kA710MagicRegs[2].value, 0x00000400u);

    CHECK_EQ(kA710MagicRegs[3].reg, REG_A7XX_UCHE_CLIENT_PF);
    CHECK_EQ(kA710MagicRegs[3].value, 0x00000084u);

    CHECK_EQ(kA710MagicRegs[4].reg, REG_A7XX_PC_MODE_CNTL);
    CHECK_EQ(kA710MagicRegs[4].value, 0x00001f1fu);

    CHECK_EQ(kA710MagicRegs[5].reg, REG_A7XX_SP_DBG_ECO_CNTL);
    CHECK_EQ(kA710MagicRegs[5].value, 0x10000000u);

    CHECK_EQ(kA710MagicRegs[8].reg, REG_A7XX_VPC_DBG_ECO_CNTL);
    CHECK_EQ(kA710MagicRegs[8].value, 0x02000000u);

    CHECK_EQ(kA710MagicRegs[9].reg, REG_A7XX_UCHE_UNKNOWN_0E12);
    CHECK_EQ(kA710MagicRegs[9].value, 0x03200000u);

    CHECK_EQ(kA710MagicRegs[10].reg, REG_A7XX_RB_UNKNOWN_8E06);
    CHECK_EQ(kA710MagicRegs[10].value, 0x00080000u);

    // Raw magic regs: 28 entries
    constexpr size_t kNumRaw = sizeof(kA710RawMagicRegs) / sizeof(kA710RawMagicRegs[0]);
    CHECK_EQ(kNumRaw, 28u);
    CHECK_EQ(kA710RawMagicRegs[0].reg, 0x0e17u);
    CHECK_EQ(kA710RawMagicRegs[0].value, 0x00040004u);
    CHECK_EQ(kA710RawMagicRegs[1].reg, 0xb602u);
    CHECK_EQ(kA710RawMagicRegs[1].value, 0x00000700u);
}

TEST(a7xx_pm4_event_tokens) {
    // Verified from adreno_pm4.xml:141-155 and gen7_reg.h:18-23
    CHECK_EQ(uint32_t(A7XX_CCU_INVALIDATE_DEPTH), 24u);
    CHECK_EQ(uint32_t(A7XX_CCU_INVALIDATE_COLOR), 25u);
    CHECK_EQ(uint32_t(A7XX_CCU_RESOLVE_CLEAN), 26u);
    CHECK_EQ(uint32_t(A7XX_CCU_FLUSH_DEPTH), 28u);
    CHECK_EQ(uint32_t(A7XX_CCU_FLUSH_COLOR), 29u);
    CHECK_EQ(uint32_t(A7XX_CCU_CLEAN_DEPTH), 32u);
    CHECK_EQ(uint32_t(A7XX_CCU_CLEAN_COLOR), 33u);
    CHECK_EQ(uint32_t(A7XX_CACHE_FLUSH7), 50u);
    CHECK_EQ(uint32_t(A7XX_CACHE_INVALIDATE7), 51u);
}

TEST(a7xx_pm4_packet_headers) {
    // Parity rules verified against pm4.hpp
    // REG_A7XX_SP_WINDOW_OFFSET = 0xab21 = 1010 1011 0010 0001 (7 ones -> odd parity bit = 0)
    // Count 1 = 1 (1 one -> odd parity bit = 0)
    // Header = 0x40000000 | 1 | (0xab21 << 8) = 0x40ab2101
    uint32_t pkt4 = pm4::type4(REG_A7XX_SP_WINDOW_OFFSET, 1);
    pm4::Header h = pm4::decode(pkt4);
    CHECK_EQ(uint32_t(h.kind), uint32_t(pm4::Header::kRegisters));
    CHECK_EQ(h.reg, REG_A7XX_SP_WINDOW_OFFSET);
    CHECK_EQ(h.count, 1u);

    // REG_A7XX_SP_CS_CONST_CONFIG = 0xa9cd = 1010 1001 1100 1101 (9 ones -> odd parity bit = 0)
    uint32_t pkt4_cs = pm4::type4(REG_A7XX_SP_CS_CONST_CONFIG, 1);
    h = pm4::decode(pkt4_cs);
    CHECK_EQ(uint32_t(h.kind), uint32_t(pm4::Header::kRegisters));
    CHECK_EQ(h.reg, REG_A7XX_SP_CS_CONST_CONFIG);
    CHECK_EQ(h.count, 1u);
}

#include "hakios-metal/fake_gpu.hpp"
#include "hakios-metal/render.hpp"
#include "hakios-metal/texture.hpp"

TEST(a7xx_render_stream_emission) {
    auto fake = std::make_unique<FakeGpu>();
    Gpu gpu(std::move(fake));
    TextureDesc desc{};
    desc.format = PixelFormat::kRGBA8Unorm;
    desc.width = 64;
    desc.height = 64;
    desc.usage = kTextureUsageRenderTarget;
    std::string why;
    auto tex = Texture::make(gpu, desc, &why);
    CHECK(tex != nullptr);

    RenderPass pass{};
    pass.color0 = tex.get();
    pass.load = LoadAction::kLoad;
    pass.store = StoreAction::kStore;

    uint8_t tableCpu[render::kTableBytes] = {};
    uint64_t tableGpu = 0x10000;

    // Test A6xx emission
    pm4::Stream streamA6xx;
    render::Options optsA6xx{};
    optsA6xx.generation = ChipGeneration::kA6xx;
    CHECK(render::encodeBegin(streamA6xx, pass, tableGpu, tableCpu, &why, optsA6xx));
    render::encodeEnd(streamA6xx, pass, tableGpu, optsA6xx);

    bool foundA6xxWindowOffset = false;
    for (size_t i = 0; i < streamA6xx.words().size();) {
        pm4::Header h = pm4::decode(streamA6xx.words()[i]);
        if (h.kind == pm4::Header::kRegisters && h.reg == a6xx::REG_A6XX_SP_WINDOW_OFFSET) {
            foundA6xxWindowOffset = true;
        }
        i += 1 + h.count;
    }
    CHECK(foundA6xxWindowOffset);

    // Test A7xx emission
    pm4::Stream streamA7xx;
    render::Options optsA7xx{};
    optsA7xx.generation = ChipGeneration::kA7xx;
    CHECK(render::encodeBegin(streamA7xx, pass, tableGpu, tableCpu, &why, optsA7xx));
    render::encodeEnd(streamA7xx, pass, tableGpu, optsA7xx);

    bool foundA7xxWindowOffset = false;
    bool foundA7xxMagicReg = false;
    bool foundA7xxRawMagicReg = false;

    for (size_t i = 0; i < streamA7xx.words().size();) {
        pm4::Header h = pm4::decode(streamA7xx.words()[i]);
        if (h.kind == pm4::Header::kRegisters) {
            if (h.reg == a7xx::REG_A7XX_SP_WINDOW_OFFSET) {
                foundA7xxWindowOffset = true;
            }
            if (h.reg == a7xx::REG_A7XX_TPL1_DBG_ECO_CNTL) {
                foundA7xxMagicReg = true;
                CHECK_EQ(streamA7xx.words()[i + 1], 0x01000000u);
            }
            if (h.reg == a7xx::kA710RawMagicRegs[0].reg) {
                foundA7xxRawMagicReg = true;
                CHECK_EQ(streamA7xx.words()[i + 1], a7xx::kA710RawMagicRegs[0].value);
            }
        }
        i += 1 + h.count;
    }

    CHECK(foundA7xxWindowOffset);
    CHECK(foundA7xxMagicReg);
    CHECK(foundA7xxRawMagicReg);
}

// 13 November, garnet: every CoreAnimation buffer hung on a wait for a value an A6xx-form 4-word CP_EVENT_WRITE never
// wrote (the A7xx writes only with WRITE_ENABLED), and CP_WAIT_MEM_GTE is A6XX only. On the A7xx a pass's stream has
// neither: every 4-word event has bit 27 and no A6xx *_TS event, and no CP_WAIT_MEM_GTE.
TEST(a7xx_events_write_and_waits_are_a7xx) {
    auto fake = std::make_unique<FakeGpu>();
    Gpu gpu(std::move(fake));
    TextureDesc desc{};
    desc.format = PixelFormat::kRGBA8Unorm;
    desc.width = 64;
    desc.height = 64;
    desc.usage = kTextureUsageRenderTarget;
    std::string why;
    auto tex = Texture::make(gpu, desc, &why);
    CHECK(tex != nullptr);
    RenderPass pass{};
    pass.color0 = tex.get();
    pass.load = LoadAction::kClear;
    pass.store = StoreAction::kStore;
    uint8_t tableCpu[render::kTableBytes] = {};
    pm4::setA7xx(true);
    pm4::Stream s;
    render::Options o{};
    o.generation = ChipGeneration::kA7xx;
    CHECK(render::encodeBegin(s, pass, 0x10000, tableCpu, &why, o));
    render::encodeEnd(s, pass, 0x10000, o);
    pm4::setA7xx(false);
    uint32_t written = 0;
    for (size_t i = 0; i < s.words().size();) {
        pm4::Header h = pm4::decode(s.words()[i]);
        CHECK(h.kind != pm4::Header::kBad);
        if (h.kind == pm4::Header::kCommand) {
            CHECK(h.opcode != a6xx::CP_WAIT_MEM_GTE);
            if (h.opcode == a6xx::CP_EVENT_WRITE && h.count == 4) {
                const uint32_t w0 = s.words()[i + 1], e = w0 & 0xff;
                CHECK((w0 & a7xx::A7XX_EVENT_WRITE_ENABLED) != 0);
                CHECK(e == a7xx::A7XX_CACHE_CLEAN || e == a6xx::RB_DONE_TS);
                written++;
            }
        }
        i += 1 + h.count;
    }
    CHECK(written > 0);
}
