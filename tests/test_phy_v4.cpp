#include "../itlwm/hal_iwx/iwx_phy_v4.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    iwx_phy_context_cmd_v4_wire phy{};
    phy.id_and_color = 0x44332211;
    phy.action = 0x88776655;
    phy.channel = 0x04030201;
    phy.band = 0x05;
    phy.width = 0x06;
    phy.ctrl_pos = 0x07;
    phy.lmac_id = 0x0c0b0a09;

    const auto *bytes = reinterpret_cast<const uint8_t *>(&phy);
    assert(sizeof(phy) == 32);
    assert(bytes[0] == 0x11 && bytes[3] == 0x44);
    assert(bytes[4] == 0x55 && bytes[7] == 0x88);
    assert(bytes[8] == 0x01 && bytes[11] == 0x04);
    assert(bytes[12] == 0x05 && bytes[13] == 0x06 &&
           bytes[14] == 0x07 && bytes[15] == 0x00);
    assert(bytes[16] == 0x09 && bytes[19] == 0x0c);
    assert(bytes[20] == 0x00 && bytes[23] == 0x00);
    assert(bytes[24] == 0x00 && bytes[27] == 0x00);
    assert(bytes[28] == 0x00 && bytes[31] == 0x00);

    iwx_rlc_config_cmd_v2_wire rlc{};
    rlc.phy_id = 0x04030201;
    rlc.rx_chain_info = 0x0000320e;
    const auto *rlc_bytes = reinterpret_cast<const uint8_t *>(&rlc);
    assert(sizeof(rlc) == 32);
    assert(rlc_bytes[0] == 0x01 && rlc_bytes[3] == 0x04);
    assert(rlc_bytes[4] == 0x0e && rlc_bytes[5] == 0x32 &&
           rlc_bytes[6] == 0x00 && rlc_bytes[7] == 0x00);
    assert(rlc_bytes[28] == 0x00 && rlc_bytes[31] == 0x00);

    return 0;
}
