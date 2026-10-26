/* SPDX-License-Identifier: GPL-2.0-only */
/* Firmware ABI based on Linux v6.12 fw/api/phy-ctxt.h and datapath.h.
 * Copyright (C) 2012-2014, 2018-2022, 2024 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2016-2017 Intel Deutschland GmbH
 * Minimal API v4 PHY/RLC definitions, kept independent for host tests.
 */
#ifndef IWX_PHY_V4_H
#define IWX_PHY_V4_H

#include <stddef.h>
#include <stdint.h>

#define IWX_RLC_CONFIG_CMD 0x08

#pragma pack(push, 1)
struct iwx_phy_context_cmd_v4_wire {
    uint32_t id_and_color;
    uint32_t action;
    uint32_t channel;
    uint8_t band;
    uint8_t width;
    uint8_t ctrl_pos;
    uint8_t channel_reserved;
    uint32_t lmac_id;
    /* PHY_CONTEXT_DATA_API_S_VER_4: RX chain field is reserved when RLC v2 is used. */
    uint32_t rxchain_info;
    uint32_t dsp_cfg_flags;
    uint32_t reserved;
};

struct iwx_rlc_config_cmd_v2_wire {
    uint32_t phy_id;
    uint32_t rx_chain_info;
    uint32_t rlc_reserved;
    uint32_t sad_chain_a;
    uint32_t sad_chain_b;
    uint32_t sad_mac_id;
    uint32_t sad_reserved;
    uint8_t flags;
    uint8_t reserved[3];
};
#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(iwx_phy_context_cmd_v4_wire) == 32,
              "PHY_CONTEXT_CMD API v4 payload size");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, id_and_color) == 0,
              "PHY_CONTEXT v4 id offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, action) == 4,
              "PHY_CONTEXT v4 action offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, channel) == 8,
              "PHY_CONTEXT v4 channel offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, band) == 12,
              "PHY_CONTEXT v4 band offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, lmac_id) == 16,
              "PHY_CONTEXT v4 lmac offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, rxchain_info) == 20,
              "PHY_CONTEXT v4 RLC-dependent field offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, dsp_cfg_flags) == 24,
              "PHY_CONTEXT v4 dsp offset");
static_assert(offsetof(iwx_phy_context_cmd_v4_wire, reserved) == 28,
              "PHY_CONTEXT v4 reserved offset");
static_assert(sizeof(iwx_rlc_config_cmd_v2_wire) == 32,
              "RLC_CONFIG_CMD API v2 payload size");
static_assert(offsetof(iwx_rlc_config_cmd_v2_wire, rx_chain_info) == 4,
              "RLC_CONFIG v2 chain offset");
static_assert(offsetof(iwx_rlc_config_cmd_v2_wire, flags) == 28,
              "RLC_CONFIG v2 flags offset");
#endif

#endif /* IWX_PHY_V4_H */
