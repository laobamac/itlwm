/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 Intel Corporation
 * Minimal Intel API 83 MLD data-path wire definitions.
 * Kept freestanding so the ABI can be checked by host-only tests.
 * Wire layout is derived from Intel's Linux firmware API definitions.
 */
#ifndef IWX_MLD_DATA_H
#define IWX_MLD_DATA_H

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define IWX_MLD_PACKED __attribute__((__packed__))
#else
#define IWX_MLD_PACKED
#endif

#define IWX_MLD_DATA_PATH_GROUP 0x05
#define IWX_MLD_RX_BAID_CONFIG  0x16
#define IWX_MLD_SCD_QUEUE_CONFIG 0x17
#define IWX_MLD_SEC_KEY         0x18

#define IWX_MLD_BAID_MAX 32
#define IWX_MLD_BAID_INVALID 0xff
#define IWX_MLD_TXQ_ADD 0
#define IWX_MLD_TXQ_REMOVE 1
#define IWX_MLD_TXQ_MODIFY 2
#define IWX_MLD_BAID_ADD 0
#define IWX_MLD_BAID_MODIFY 1
#define IWX_MLD_BAID_REMOVE 2
/* SEC_KEY uses FW_CTXT_ACTION, unlike the zero-based SCD/BAID actions. */
#define IWX_MLD_KEY_ADD 1
#define IWX_MLD_KEY_MODIFY 2
#define IWX_MLD_KEY_REMOVE 3

#define IWX_MLD_KEY_CIPHER_CCMP 0x02
#define IWX_MLD_KEY_CIPHER_MASK 0x07
#define IWX_MLD_KEY_NO_TX 0x08
#define IWX_MLD_KEY_SIZE 0x10
#define IWX_MLD_KEY_MFP 0x20
#define IWX_MLD_KEY_MCAST 0x40

/* `sta_mask` is a bitmap: station ID n is represented by (1u << n). */
static inline int iwx_mld_station_mask(uint32_t station_id, uint32_t *mask)
{
    if (!mask || station_id >= 32)
        return 0;
    *mask = UINT32_C(1) << station_id;
    return 1;
}

struct iwx_mld_txq_cfg_cmd {
    uint32_t operation;
    union {
        struct IWX_MLD_PACKED {
            uint32_t sta_mask;
            uint8_t tid;
            uint8_t reserved[3];
            uint32_t flags;
            uint32_t cb_size;
            uint64_t bc_dram_addr;
            uint64_t tfdq_dram_addr;
        } add;
        struct IWX_MLD_PACKED { uint32_t sta_mask, tid; } remove;
        struct IWX_MLD_PACKED {
            uint32_t old_sta_mask, tid, new_sta_mask;
        } modify;
    } IWX_MLD_PACKED u;
} IWX_MLD_PACKED;

struct iwx_mld_txq_cfg_rsp {
    uint16_t queue_number;
    uint16_t flags;
    uint16_t write_pointer;
    uint16_t reserved;
} IWX_MLD_PACKED;

struct iwx_mld_rx_baid_cfg_cmd {
    uint32_t action;
    union {
        struct IWX_MLD_PACKED {
            uint32_t sta_id_mask;
            uint8_t tid;
            uint8_t reserved[3];
            uint16_t ssn;
            uint16_t win_size;
        } alloc;
        struct IWX_MLD_PACKED {
            uint32_t old_sta_id_mask, new_sta_id_mask, tid;
        } modify;
        struct IWX_MLD_PACKED { uint32_t baid; } remove_v1;
        struct IWX_MLD_PACKED { uint32_t sta_id_mask, tid; } remove;
    } IWX_MLD_PACKED u;
} IWX_MLD_PACKED;

struct iwx_mld_rx_baid_cfg_resp { uint32_t baid; } IWX_MLD_PACKED;

struct iwx_mld_sec_key_cmd {
    uint32_t action;
    union {
        struct IWX_MLD_PACKED {
            uint32_t sta_mask, key_id, key_flags;
            uint8_t key[32];
            uint8_t tkip_mic_rx_key[8], tkip_mic_tx_key[8];
            uint64_t rx_seq, tx_seq;
        } add;
        struct IWX_MLD_PACKED {
            uint32_t old_sta_mask, new_sta_mask, key_id, key_flags;
        } modify;
        struct IWX_MLD_PACKED { uint32_t sta_mask, key_id, key_flags; } remove;
    } IWX_MLD_PACKED u;
} IWX_MLD_PACKED;

/* Explicit byte-order writes avoid host-endian assumptions in tests/helpers. */
static inline void iwx_mld_put_le16(void *dst, uint16_t value)
{
    uint8_t *p = (uint8_t *)dst;
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}
static inline void iwx_mld_put_le32(void *dst, uint32_t value)
{
    uint8_t *p = (uint8_t *)dst;
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}
static inline void iwx_mld_put_le64(void *dst, uint64_t value)
{
    uint8_t *p = (uint8_t *)dst;
    iwx_mld_put_le32(p, (uint32_t)value);
    iwx_mld_put_le32(p + 4, (uint32_t)(value >> 32));
}
static inline uint16_t iwx_mld_get_le16(const void *src)
{
    const uint8_t *p = (const uint8_t *)src;
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t iwx_mld_get_le32(const void *src)
{
    const uint8_t *p = (const uint8_t *)src;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Build SCD_QUEUE_CONFIG v3 ADD without partially changing cmd on bad input. */
static inline int iwx_mld_build_txq_add(struct iwx_mld_txq_cfg_cmd *cmd,
                                        uint32_t station_id, uint32_t tid,
                                        uint32_t cb_size, uint64_t bc_addr,
                                        uint64_t tfdq_addr)
{
    struct iwx_mld_txq_cfg_cmd built = {};
    uint32_t sta_mask;

    if (!cmd || tid > 15 || !iwx_mld_station_mask(station_id, &sta_mask))
        return 0;
    iwx_mld_put_le32(&built.operation, IWX_MLD_TXQ_ADD);
    iwx_mld_put_le32(&built.u.add.sta_mask, sta_mask);
    built.u.add.tid = (uint8_t)tid;
    iwx_mld_put_le32(&built.u.add.cb_size, cb_size);
    iwx_mld_put_le64(&built.u.add.bc_dram_addr, bc_addr);
    iwx_mld_put_le64(&built.u.add.tfdq_dram_addr, tfdq_addr);
    *cmd = built;
    return 1;
}

#if defined(__cplusplus)
static_assert(IWX_MLD_DATA_PATH_GROUP == 0x05, "API 83 firmware group");
static_assert(IWX_MLD_RX_BAID_CONFIG == 0x16 &&
              IWX_MLD_SCD_QUEUE_CONFIG == 0x17 && IWX_MLD_SEC_KEY == 0x18,
              "API 83 data-path opcodes");
static_assert(sizeof(iwx_mld_txq_cfg_cmd) == 36, "SCD_QUEUE_CONFIG v3 size");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, operation) == 0, "txq operation offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u) == 4, "txq union offset");
static_assert(sizeof(iwx_mld_txq_cfg_cmd::u.add) == 32, "txq add size");
static_assert(sizeof(iwx_mld_txq_cfg_cmd::u.remove) == 8, "txq remove size");
static_assert(sizeof(iwx_mld_txq_cfg_cmd::u.modify) == 12, "txq modify size");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.sta_mask) == 4, "txq sta_mask offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.tid) == 8, "txq tid offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.flags) == 12, "txq flags offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.cb_size) == 16, "txq cb_size offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.bc_dram_addr) == 20, "txq bc addr offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.add.tfdq_dram_addr) == 28, "txq tfd addr offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.remove.sta_mask) == 4, "txq remove station mask offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.remove.tid) == 8, "txq remove TID offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.modify.old_sta_mask) == 4, "txq modify old mask offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.modify.tid) == 8, "txq modify TID offset");
static_assert(offsetof(iwx_mld_txq_cfg_cmd, u.modify.new_sta_mask) == 12, "txq modify new mask offset");
static_assert(sizeof(iwx_mld_txq_cfg_rsp) == 8, "SCD_QUEUE_CONFIG response size");
static_assert(offsetof(iwx_mld_txq_cfg_rsp, queue_number) == 0, "txq response qid offset");
static_assert(offsetof(iwx_mld_txq_cfg_rsp, flags) == 2, "txq response flags offset");
static_assert(offsetof(iwx_mld_txq_cfg_rsp, write_pointer) == 4, "txq write pointer offset");
static_assert(offsetof(iwx_mld_txq_cfg_rsp, reserved) == 6, "txq response reserved offset");
static_assert(sizeof(iwx_mld_rx_baid_cfg_cmd) == 16, "RX_BAID v2 size");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u) == 4, "baid union offset");
static_assert(sizeof(iwx_mld_rx_baid_cfg_cmd::u.alloc) == 12, "baid add size");
static_assert(sizeof(iwx_mld_rx_baid_cfg_cmd::u.modify) == 12, "baid modify size");
static_assert(sizeof(iwx_mld_rx_baid_cfg_cmd::u.remove_v1) == 4, "baid v1 remove size");
static_assert(sizeof(iwx_mld_rx_baid_cfg_cmd::u.remove) == 8, "baid v2 remove size");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.alloc.tid) == 8, "baid tid offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.alloc.ssn) == 12, "baid ssn offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.alloc.win_size) == 14, "baid window offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.modify.old_sta_id_mask) == 4, "baid modify old mask offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.modify.new_sta_id_mask) == 8, "baid modify new mask offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.modify.tid) == 12, "baid modify TID offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.remove.sta_id_mask) == 4, "baid remove mask offset");
static_assert(offsetof(iwx_mld_rx_baid_cfg_cmd, u.remove.tid) == 8, "baid remove TID offset");
static_assert(sizeof(iwx_mld_rx_baid_cfg_resp) == 4, "RX_BAID response size");
static_assert(offsetof(iwx_mld_rx_baid_cfg_resp, baid) == 0, "baid response offset");
static_assert(sizeof(iwx_mld_sec_key_cmd) == 80, "SEC_KEY v1 size");
static_assert(IWX_MLD_KEY_ADD == 1 && IWX_MLD_KEY_MODIFY == 2 &&
              IWX_MLD_KEY_REMOVE == 3, "SEC_KEY FW context action values");
static_assert(offsetof(iwx_mld_sec_key_cmd, u) == 4, "sec key union offset");
static_assert(sizeof(iwx_mld_sec_key_cmd::u.add) == 76, "sec key add size");
static_assert(sizeof(iwx_mld_sec_key_cmd::u.modify) == 16, "sec key modify size");
static_assert(sizeof(iwx_mld_sec_key_cmd::u.remove) == 12, "sec key remove size");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.sta_mask) == 4, "sec key station mask offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.key_id) == 8, "sec key id offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.key_flags) == 12, "sec key flags offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.key) == 16, "sec key bytes offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.tkip_mic_rx_key) == 48, "sec key RX MIC offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.tkip_mic_tx_key) == 56, "sec key TX MIC offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.rx_seq) == 64, "sec key rx sequence offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.add.tx_seq) == 72, "sec key tx sequence offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.modify.old_sta_mask) == 4, "sec key modify old mask offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.modify.new_sta_mask) == 8, "sec key modify new mask offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.modify.key_id) == 12, "sec key modify ID offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.modify.key_flags) == 16, "sec key modify flags offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.remove.sta_mask) == 4, "sec key remove mask offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.remove.key_id) == 8, "sec key remove ID offset");
static_assert(offsetof(iwx_mld_sec_key_cmd, u.remove.key_flags) == 12, "sec key remove flags offset");
#endif

#endif
