#include "../itlwm/hal_iwx/iwx_mld_data.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

int main()
{
    uint32_t mask = 0;
    assert(iwx_mld_station_mask(0, &mask) && mask == 1u);
    assert(iwx_mld_station_mask(7, &mask) && mask == (1u << 7));
    assert(iwx_mld_station_mask(31, &mask) && mask == 0x80000000u);
    assert(!iwx_mld_station_mask(32, &mask));
    assert(!iwx_mld_station_mask(1, nullptr));

    uint8_t wire[8] = {};
    iwx_mld_put_le16(wire, 0x1234);
    iwx_mld_put_le32(wire + 2, 0x89abcdefu);
    assert(wire[0] == 0x34 && wire[1] == 0x12);
    assert(wire[2] == 0xef && wire[3] == 0xcd &&
           wire[4] == 0xab && wire[5] == 0x89);
    assert(iwx_mld_get_le16(wire) == 0x1234);
    assert(iwx_mld_get_le32(wire + 2) == 0x89abcdefu);
    iwx_mld_put_le64(wire, UINT64_C(0x0123456789abcdef));
    const uint8_t le64[] = {0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01};
    assert(std::memcmp(wire, le64, sizeof(le64)) == 0);

    iwx_mld_txq_cfg_cmd txq = {};
    iwx_mld_put_le32(&txq.operation, IWX_MLD_TXQ_ADD);
    iwx_mld_put_le32(&txq.u.add.sta_mask, 1u << 10);
    txq.u.add.tid = 5;
    iwx_mld_put_le32(&txq.u.add.flags, 0x11223344);
    iwx_mld_put_le32(&txq.u.add.cb_size, 0x55667788);
    iwx_mld_put_le64(&txq.u.add.bc_dram_addr, UINT64_C(0x0123456789abcdef));
    iwx_mld_put_le64(&txq.u.add.tfdq_dram_addr, UINT64_C(0xfedcba9876543210));
    auto *q = reinterpret_cast<const uint8_t *>(&txq);
    assert(iwx_mld_get_le32(q) == 0);
    assert(iwx_mld_get_le32(q + 4) == (1u << 10));
    assert(q[8] == 5 && q[9] == 0 && q[10] == 0 && q[11] == 0);
    assert(iwx_mld_get_le32(q + 12) == 0x11223344);
    assert(iwx_mld_get_le32(q + 16) == 0x55667788);
    assert(iwx_mld_get_le32(q + 20) == 0x89abcdefu);
    assert(iwx_mld_get_le32(q + 28) == 0x76543210u);

    iwx_mld_txq_cfg_cmd built_q = {};
    assert(iwx_mld_build_txq_add(&built_q, 0, 15, 0x1234,
                                 UINT64_C(0x0123456789abcdef),
                                 UINT64_C(0xfedcba9876543210)));
    assert(iwx_mld_get_le32(&built_q.u.add.sta_mask) == 1u);
    assert(built_q.u.add.tid == 15);
    assert(iwx_mld_get_le32(&built_q.u.add.cb_size) == 0x1234);
    assert(iwx_mld_get_le32((const uint8_t *)&built_q + 20) == 0x89abcdefu);
    assert(iwx_mld_build_txq_add(&built_q, 1, 0, 0x5678, 1, 2));
    assert(iwx_mld_get_le32(&built_q.u.add.sta_mask) == 2u);
    std::memset(&built_q, 0xa5, sizeof(built_q));
    uint8_t saved_invalid[sizeof(built_q)];
    std::memcpy(saved_invalid, &built_q, sizeof(built_q));
    assert(!iwx_mld_build_txq_add(&built_q, 32, 0, 1, 2, 3));
    assert(std::memcmp(&built_q, saved_invalid, sizeof(built_q)) == 0);
    assert(!iwx_mld_build_txq_add(&built_q, 0, 16, 1, 2, 3));
    assert(std::memcmp(&built_q, saved_invalid, sizeof(built_q)) == 0);
    assert(!iwx_mld_build_txq_add(nullptr, 0, 0, 1, 2, 3));

    iwx_mld_rx_baid_cfg_cmd ba = {};
    iwx_mld_put_le32(&ba.action, IWX_MLD_BAID_ADD);
    iwx_mld_put_le32(&ba.u.alloc.sta_id_mask, 1u << 9);
    ba.u.alloc.tid = 7;
    iwx_mld_put_le16(&ba.u.alloc.ssn, 0x0abc);
    iwx_mld_put_le16(&ba.u.alloc.win_size, 64);
    auto *b = reinterpret_cast<const uint8_t *>(&ba);
    assert(iwx_mld_get_le32(b + 4) == (1u << 9));
    assert(b[8] == 7 && b[9] == 0 && b[10] == 0 && b[11] == 0);
    assert(iwx_mld_get_le16(b + 12) == 0x0abc);
    assert(iwx_mld_get_le16(b + 14) == 64);

    /* Largest sequence number and current net80211 BA window. */
    std::memset(&ba, 0, sizeof(ba));
    iwx_mld_put_le32(&ba.action, IWX_MLD_BAID_ADD);
    iwx_mld_put_le32(&ba.u.alloc.sta_id_mask, 1u);
    ba.u.alloc.tid = 7;
    iwx_mld_put_le16(&ba.u.alloc.ssn, 0x0fff);
    iwx_mld_put_le16(&ba.u.alloc.win_size, 64);
    assert(iwx_mld_get_le16(reinterpret_cast<const uint8_t *>(&ba) + 12) == 0x0fff);
    assert(iwx_mld_get_le16(reinterpret_cast<const uint8_t *>(&ba) + 14) == 64);

    /* API v2 REMOVE identifies a station/TID, not a BAID. */
    std::memset(&ba, 0, sizeof(ba));
    iwx_mld_put_le32(&ba.action, IWX_MLD_BAID_REMOVE);
    iwx_mld_put_le32(&ba.u.remove.sta_id_mask, 1u);
    iwx_mld_put_le32(&ba.u.remove.tid, 7);
    assert(iwx_mld_get_le32(reinterpret_cast<const uint8_t *>(&ba)) == 2);
    assert(iwx_mld_get_le32(reinterpret_cast<const uint8_t *>(&ba) + 4) == 1);
    assert(iwx_mld_get_le32(reinterpret_cast<const uint8_t *>(&ba) + 8) == 7);
    uint32_t max_baid = IWX_MLD_BAID_MAX - 1;
    iwx_mld_put_le32(wire, max_baid);
    assert(iwx_mld_get_le32(wire) == 31);

    iwx_mld_sec_key_cmd key = {};
    iwx_mld_put_le32(&key.action, IWX_MLD_KEY_ADD);
    iwx_mld_put_le32(&key.u.add.sta_mask, 1u << 10);
    iwx_mld_put_le32(&key.u.add.key_id, 1);
    iwx_mld_put_le32(&key.u.add.key_flags,
                     IWX_MLD_KEY_CIPHER_CCMP | IWX_MLD_KEY_MCAST);
    for (unsigned i = 0; i < 16; ++i)
        key.u.add.key[i] = static_cast<uint8_t>(i);
    iwx_mld_put_le64(&key.u.add.rx_seq, UINT64_C(0x010203040506));
    iwx_mld_put_le64(&key.u.add.tx_seq, UINT64_C(0x0a0b0c0d0e0f));
    auto *k = reinterpret_cast<const uint8_t *>(&key);
    assert(iwx_mld_get_le32(k) == 1); // Linux FW_CTXT_ACTION_ADD.
    assert(iwx_mld_get_le32(k + 4) == (1u << 10));
    assert(iwx_mld_get_le32(k + 8) == 1);
    assert(iwx_mld_get_le32(k + 12) == (IWX_MLD_KEY_CIPHER_CCMP | IWX_MLD_KEY_MCAST));
    for (unsigned i = 0; i < 16; ++i) assert(k[16 + i] == i);
    assert(iwx_mld_get_le32(k + 64) == 0x03040506u);
    assert(iwx_mld_get_le32(k + 72) == 0x0c0d0e0fu);
    iwx_mld_put_le32(&key.action, IWX_MLD_KEY_REMOVE);
    assert(iwx_mld_get_le32(&key.action) == 3); // Linux FW_CTXT_ACTION_REMOVE.
    return 0;
}
