// SPDX-License-Identifier: GPL-2.0-or-later
#include "../itlwm/hal_iwx/iwx_pnvm.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
using Bytes = std::vector<uint8_t>;
static void le32(Bytes &b, uint32_t x) {
    for (int i=0;i<4;i++) b.push_back((x >> (i*8)) & 255);
}
static void tlv(Bytes &b, uint32_t type, Bytes data) {
    le32(b,type); le32(b,(uint32_t)data.size());
    b.insert(b.end(),data.begin(),data.end());
    while (b.size()%4) b.push_back(0);
}
static Bytes section(uint32_t sku0=1, uint32_t hw=0x2010044) {
    Bytes b, sku; le32(sku,sku0); le32(sku,2); le32(sku,3); tlv(b,64,sku);
    Bytes h; le32(h,hw); tlv(b,58,h);
    Bytes ver; le32(ver,0x12345678); tlv(b,62,ver);
    return b;
}
static void chunk(Bytes &b, unsigned len=3) {
    Bytes d; le32(d,0); for(unsigned i=0;i<len;i++) d.push_back(i+1); tlv(b,19,d);
}
static iwx_pnvm_result parse(const Bytes &b, iwx_pnvm_image &i) {
    const uint32_t sku[]={1,2,3};
    return iwx_pnvm_parse(b.data(),b.size(),sku,0x44,0x201,&i);
}
int main(int argc,char **argv) {
    // The observed 0x50001 is opaque telemetry, not a zero-success errno.
    const uint8_t observed_completion[] = {1, 0, 5, 0};
    uint32_t completion = 0;
    assert(iwx_pnvm_completion_status(observed_completion, 4, &completion));
    assert(completion == 0x50001);
    const uint8_t extended_completion[] = {0xef, 0xbe, 0xad, 0xde, 0};
    assert(iwx_pnvm_completion_status(extended_completion, 5, &completion));
    assert(completion == 0xdeadbeef);
    for (size_t n = 0; n < 4; n++) {
        assert(!iwx_pnvm_completion_status(observed_completion, n, &completion));
        assert(completion == 0xdeadbeef);
    }
    assert(!iwx_pnvm_completion_status(nullptr, 4, &completion));
    assert(!iwx_pnvm_completion_status(observed_completion, 4, nullptr));
    const uint8_t zero_completion[4] = {};
    assert(iwx_pnvm_completion_status(zero_completion, 4, &completion));
    assert(completion == 0);
    char filename[64];
    assert(iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83.ucode", filename, sizeof(filename)));
    assert(strcmp(filename, "iwlwifi-ma-b0-gf-a0.pnvm") == 0);
    assert(iwx_pnvm_filename("iwlwifi-so-a0-gf4-a0-68.ucode", filename, sizeof(filename)));
    assert(strcmp(filename, "iwlwifi-so-a0-gf4-a0.pnvm") == 0);
    char exact[sizeof("iwlwifi-ma-b0-gf-a0.pnvm")];
    assert(iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83.ucode", exact, sizeof(exact)));
    assert(strcmp(exact, "iwlwifi-ma-b0-gf-a0.pnvm") == 0);
    char too_short[sizeof("iwlwifi-ma-b0-gf-a0.pnvm") - 1];
    memset(too_short, 0x55, sizeof(too_short));
    assert(!iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83.ucode", too_short, sizeof(too_short)));
    for (auto c : too_short) assert(c == 0x55);
    assert(!iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0.ucode", filename, sizeof(filename)));
    assert(!iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83.bad", filename, sizeof(filename)));
    assert(!iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83", filename, sizeof(filename)));
    assert(!iwx_pnvm_filename(nullptr, filename, sizeof(filename)));
    assert(!iwx_pnvm_filename("iwlwifi-ma-b0-gf-a0-83.ucode", nullptr, sizeof(filename)));
    uint8_t table[IWX_PNVM_DMA_TABLE_SIZE];
    const uint64_t addresses[] = {UINT64_C(0x12345000), UINT64_C(0x23456000)};
    const uint32_t lengths[] = {1656, 12060};
    memset(table, 0xa5, sizeof(table));
    assert(iwx_pnvm_build_dma_table(table, sizeof(table), addresses, lengths, 2));
    assert(iwx_pnvm_le32(table) == 0x12345000 && iwx_pnvm_le32(table + 4) == 0);
    assert(iwx_pnvm_le32(table + 8) == 0x23456000 && iwx_pnvm_le32(table + 12) == 0);
    for (size_t n = 16; n < sizeof(table); n++) assert(table[n] == 0);
    assert(iwx_pnvm_dma_region_valid(UINT64_C(0xfffff000), 4096));
    assert(!iwx_pnvm_dma_region_valid(UINT64_C(0xfffff000), 4097));
    assert(!iwx_pnvm_dma_region_valid(UINT64_MAX - 4095, 8192));
    assert(!iwx_pnvm_dma_region_valid(0, 4096));
    assert(!iwx_pnvm_dma_region_valid(0x1000, 0));
    const uint64_t old_inline_addresses[] = {0x1000 + 512, 0x1000 + 2176};
    memset(table, 0xa5, sizeof(table));
    assert(!iwx_pnvm_build_dma_table(table, sizeof(table), old_inline_addresses, lengths, 2));
    for (auto c : table) assert(c == 0xa5);
    assert(!iwx_pnvm_build_dma_table(table, sizeof(table)-1, addresses, lengths, 2));
    assert(!iwx_pnvm_build_dma_table(table, sizeof(table), addresses, lengths, 0));
    assert(!iwx_pnvm_build_dma_table(table, sizeof(table), addresses, lengths, 65));
    iwx_pnvm_image i;
    auto b=section(); chunk(b,3); chunk(b,65);
    assert(parse(b,i)==IWX_PNVM_OK && i.count==2 && i.size==68 && i.version==0x12345678);
    assert(i.lengths[0]==3 && i.lengths[1]==65 && i.chunks[0][2]==3);
    auto bad=b; bad.pop_back(); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=b; bad.push_back(0); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=section(8); chunk(bad); assert(parse(bad,i)==IWX_PNVM_NOT_FOUND);
    bad=section(1,0x2000044); chunk(bad); assert(parse(bad,i)==IWX_PNVM_NOT_FOUND);
    bad=section(); tlv(bad,19,{0,0,0}); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=section(); tlv(bad,64,{0,0,0,0}); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=section(); tlv(bad,19,{0,0,0,0}); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=section(); for(int n=0;n<64;n++) chunk(bad); assert(parse(bad,i)==IWX_PNVM_OK && i.count==64);
    chunk(bad); assert(parse(bad,i)==IWX_PNVM_INVALID);
    bad=section(); Bytes sep; le32(sep,0xddddeeee); tlv(bad,19,sep); chunk(bad);
    assert(parse(bad,i)==IWX_PNVM_OK && i.count==1);
    bad=section(8); chunk(bad); bad.insert(bad.end(),b.begin(),b.end());
    assert(parse(bad,i)==IWX_PNVM_OK && i.size==68);
    bad=section(); le32(bad,19); le32(bad,UINT32_MAX); assert(parse(bad,i)==IWX_PNVM_INVALID);
    // Every truncation must be bounded, including a truncated TLV header/padding.
    for(size_t n=0;n<b.size();n++) { auto truncated=Bytes(b.begin(),b.begin()+n); parse(truncated,i); }
    // Run the production parser against every SKU/HW pair in supplied real PNVMs.
    unsigned images=0;
    bool observed_mtl_sku = false;
    for(int a=1;a<argc;a++) {
        std::ifstream f(argv[a],std::ios::binary); assert(f);
        Bytes raw((std::istreambuf_iterator<char>(f)),{});
        uint32_t sku[3]={};
        for(size_t p=0;p+8<=raw.size();) {
            auto type=iwx_pnvm_le32(raw.data()+p), n=iwx_pnvm_le32(raw.data()+p+4);
            size_t end=p+8+((size_t(n)+3)&~size_t(3)); assert(end<=raw.size());
            auto d=raw.data()+p+8;
            if(type==64 && n==12) for(int j=0;j<3;j++) sku[j]=iwx_pnvm_le32(d+j*4);
            if(type==58 && n>=4) {
                auto hw=iwx_pnvm_le32(d);
                assert(iwx_pnvm_parse(raw.data(),raw.size(),sku,hw&65535,hw>>16,&i)==IWX_PNVM_OK);
                assert(i.count && i.size); images++;
                if (sku[0] == 0x000510d1 && sku[1] == 0 && sku[2] == 0 && hw == 0x010d0044) {
                    assert(i.count == 2 && i.size == 13716 && i.version == 0x4f8f0b9a);
                    observed_mtl_sku = true;
                }
            }
            p=end;
        }
    }
    if (argc > 1) assert(observed_mtl_sku);
    std::cout << "PNVM parser boundary tests passed; real images: " << images << "\n";
}
