/* SPDX-License-Identifier: GPL-2.0-only */
/* Firmware ABI derived from Linux v6.6/v6.12 fw/api/mac-cfg.h.
 * Copyright (C) 2012-2014, 2018-2019, 2021-2024 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2016-2017 Intel Deutschland GmbH
 */
#ifndef IWX_MLD_MAC_H
#define IWX_MLD_MAC_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct iwx_mld_client_v2 {
    uint8_t is_assoc, esr_transition_timeout;
    uint16_t medium_sync_delay, assoc_id, reserved1, data_policy, reserved2;
    uint32_t ctwin;
} __attribute__((packed));

struct iwx_mld_mac_v2 {
    uint32_t id_and_color, action, mac_type;
    uint8_t local_addr[6];
    uint16_t reserved_addr;
    uint32_t filter_flags;
    uint16_t he_support, he_ap_support;
    uint32_t eht_support, nic_not_ack_enabled;
    struct iwx_mld_client_v2 client;
} __attribute__((packed));

struct iwx_mld_ac_qos {
    uint16_t cw_min, cw_max;
    uint8_t aifsn, fifos_mask;
    uint16_t edca_txop;
} __attribute__((packed));

struct iwx_mld_he_backoff {
    uint16_t cwmin, cwmax, aifsn, mu_time;
} __attribute__((packed));

struct iwx_mld_link_v1 {
    uint32_t action, link_id, mac_id, phy_id;
    uint8_t local_addr[6];
    uint16_t reserved_addr;
    uint32_t modify_mask, active, listen_lmac, cck_rates, ofdm_rates;
    uint32_t preamble, slot, protection, qos;
    struct iwx_mld_ac_qos ac[5];
    uint8_t htc_ext, ecwmin, ecwmax, ndp;
    struct iwx_mld_he_backoff trig[4];
    uint32_t bi, dtim;
    uint16_t puncture, rts;
    uint32_t flags, flags_mask;
    uint8_t ref_addr[6];
    uint16_t reserved_ref;
    uint8_t bssid_index, bss_color, spec_link_id, ul_mu_data_disable;
    uint8_t ibss_addr[6];
    uint16_t reserved_ibss;
    uint32_t reserved3[8];
} __attribute__((packed));

struct iwx_mld_sta_v1 {
    uint32_t sta_id, link_id;
    uint8_t peer_mld[6];
    uint16_t reserved_mld;
    uint8_t peer_link[6];
    uint16_t reserved_link;
    uint32_t type, assoc_id, beamform, mfp, mimo, mimo_protection;
    uint32_t ack_enabled, trig_alloc, spacing, max_size, sp_length, uapsd;
    uint8_t pkt_ext[2][5][2];
    uint32_t htc;
} __attribute__((packed));

struct iwx_mld_aux_v1 {
    uint32_t sta_id, lmac_id;
    uint8_t mac_addr[6];
    uint16_t reserved;
} __attribute__((packed));

enum {
    IWX_MLD_MODIFY_ACTIVE = 1U << 0,
    IWX_MLD_MODIFY_RATES = 1U << 1,
    IWX_MLD_MODIFY_PROTECTION = 1U << 2,
    IWX_MLD_MODIFY_QOS = 1U << 3,
    IWX_MLD_MODIFY_BEACON = 1U << 4,
    IWX_MLD_MODIFY_HE = 1U << 5,
    IWX_MLD_FILTER_GROUP = 1U << 2,
    IWX_MLD_FILTER_BEACON = 1U << 3
};

static_assert(sizeof(iwx_mld_mac_v2) == 52, "MAC_CONFIG v2 ABI");
static_assert(offsetof(iwx_mld_mac_v2, client) == 36, "MAC client offset");
static_assert(sizeof(iwx_mld_link_v1) == 208, "LINK_CONFIG v1 ABI");
static_assert(offsetof(iwx_mld_link_v1, ac) == 60, "LINK QoS offset");
static_assert(offsetof(iwx_mld_link_v1, bi) == 136, "LINK timing offset");
static_assert(sizeof(iwx_mld_sta_v1) == 96, "STA_CONFIG v1 ABI");
static_assert(offsetof(iwx_mld_sta_v1, htc) == 92, "STA HTC offset");
static_assert(sizeof(iwx_mld_aux_v1) == 16, "AUX_STA v1 ABI");

static inline void iwx_mld_put16(void *p, uint16_t value)
{
    uint8_t *out = (uint8_t *)p;
    out[0] = value;
    out[1] = value >> 8;
}

static inline void iwx_mld_put32(void *p, uint32_t value)
{
    uint8_t *out = (uint8_t *)p;
    for (unsigned i = 0; i < 4; i++)
        out[i] = value >> (8 * i);
}

static inline uint32_t iwx_mld_get32(const void *p)
{
    const uint8_t *in = (const uint8_t *)p;
    return uint32_t(in[0]) | (uint32_t(in[1]) << 8) |
        (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
}

/* Add the MAC-owned link resource before a scan, without assigning a PHY. */
template<class Send>
static int iwx_mld_add_link_inactive(
    const struct iwx_mld_link_v1 &configured, bool &present, bool &active,
    bool &timing_set, Send send)
{
    if (present)
        return 0;

    struct iwx_mld_link_v1 cmd = {};
    cmd.link_id = configured.link_id;
    cmd.mac_id = configured.mac_id;
    memcpy(cmd.local_addr, configured.local_addr, sizeof(cmd.local_addr));
    iwx_mld_put32(&cmd.phy_id, UINT32_MAX);
    iwx_mld_put32(&cmd.action, 1);

    int err = send(cmd);
    if (!err) {
        present = true;
        active = false;
        timing_set = false;
    }
    return err;
}

/* The same lifecycle runs in the kext and in fault-injected host tests. */
template<class Send>
static int iwx_mld_apply_link(const struct iwx_mld_link_v1 &configured,
    uint32_t action, bool beacon_timing, bool &present, bool &active,
    bool &timing_set, Send send)
{
    struct iwx_mld_link_v1 cmd = {};
    auto remove = [&]() -> int {
        struct iwx_mld_link_v1 rm = {};
        iwx_mld_put32(&rm.action, 3);
        rm.link_id = configured.link_id;
        iwx_mld_put32(&rm.phy_id, UINT32_MAX);
        int err = send(rm);
        if (!err) present = active = timing_set = false;
        return err;
    };
    if (action == 3) {
        if (!present) return 0;
        if (active) {
            cmd = configured;
            iwx_mld_put32(&cmd.action, 2);
            iwx_mld_put32(&cmd.modify_mask, IWX_MLD_MODIFY_ACTIVE);
            iwx_mld_put32(&cmd.active, 0);
            int err = send(cmd);
            if (err) return err;
            active = false;
        }
        return remove();
    }
    if (action == 1) {
        if (present && active) return 0;
        int err = iwx_mld_add_link_inactive(configured, present, active,
                                             timing_set, send);
        if (err) return err;
        cmd = configured;
        iwx_mld_put32(&cmd.action, 2);
        iwx_mld_put32(&cmd.modify_mask, 0);
        iwx_mld_put32(&cmd.active, 0);
        err = send(cmd); /* Assign PHY while still inactive. */
        if (err) { remove(); return err; }
        cmd = configured;
        iwx_mld_put32(&cmd.action, 2);
        iwx_mld_put32(&cmd.active, 1);
        iwx_mld_put32(&cmd.modify_mask, IWX_MLD_MODIFY_ACTIVE |
            IWX_MLD_MODIFY_RATES | IWX_MLD_MODIFY_PROTECTION | IWX_MLD_MODIFY_QOS);
        err = send(cmd);
        if (err) { remove(); return err; }
        active = true;
        return 0;
    }
    cmd = configured;
    iwx_mld_put32(&cmd.action, 2);
    iwx_mld_put32(&cmd.active, active);
    uint32_t changes = IWX_MLD_MODIFY_RATES | IWX_MLD_MODIFY_PROTECTION |
        IWX_MLD_MODIFY_QOS | iwx_mld_get32(&configured.modify_mask);
    if (beacon_timing && !timing_set) changes |= IWX_MLD_MODIFY_BEACON;
    iwx_mld_put32(&cmd.modify_mask, changes);
    int err = send(cmd);
    if (!err && beacon_timing) timing_set = true;
    return err;
}

static inline bool iwx_mld_build_mac(struct iwx_mld_mac_v2 *cmd,
    uint32_t id, uint32_t action, uint32_t mac_type, const uint8_t addr[6],
    bool associated, uint16_t aid, bool he)
{
    if (id > 3 || action < 1 || action > 3 || !addr)
        return false;
    memset(cmd, 0, sizeof(*cmd));
    iwx_mld_put32(&cmd->id_and_color, id);
    iwx_mld_put32(&cmd->action, action);
    if (action == 3)
        return true; /* Linux removes with only ID and action populated. */
    iwx_mld_put32(&cmd->mac_type, mac_type);
    memcpy(cmd->local_addr, addr, 6);
    iwx_mld_put32(&cmd->filter_flags, IWX_MLD_FILTER_GROUP |
        (associated ? 0 : IWX_MLD_FILTER_BEACON));
    iwx_mld_put16(&cmd->he_support, he ? 1 : 0);
    iwx_mld_put32(&cmd->nic_not_ack_enabled, 1);
    cmd->client.is_assoc = associated;
    iwx_mld_put16(&cmd->client.assoc_id, aid & 0x3fff);
    return true;
}

/* Firmware rate bits index physical rates, unlike the driver's rate table. */
static inline void iwx_mld_basic_rates(const uint8_t *rates, unsigned count,
    uint32_t *cck, uint32_t *ofdm)
{
    const uint8_t cck_values[] = {2, 4, 11, 22};
    const uint8_t ofdm_values[] = {12, 18, 24, 36, 48, 72, 96, 108};
    unsigned lowest_cck = 4, lowest_ofdm = 8;
    *cck = *ofdm = 0;
    for (unsigned i = 0; rates && i < count; i++) {
        if (!(rates[i] & 0x80))
            continue;
        uint8_t rate = rates[i] & 0x7f;
        for (unsigned n = 0; n < 4; n++) {
            if (rate == cck_values[n]) {
                *cck |= 1U << n;
                if (n < lowest_cck) lowest_cck = n;
            }
        }
        for (unsigned n = 0; n < 8; n++) {
            if (rate == ofdm_values[n]) {
                *ofdm |= 1U << n;
                if (n < lowest_ofdm) lowest_ofdm = n;
            }
        }
    }
    if (lowest_ofdm > 4) *ofdm |= 1U << 4; /* 24 Mb/s */
    if (lowest_ofdm > 2) *ofdm |= 1U << 2; /* 12 Mb/s */
    *ofdm |= 1; /* 6 Mb/s */
    for (unsigned n = 1; n < lowest_cck; n++) *cck |= 1U << n;
    *cck |= 1;
}

#endif
