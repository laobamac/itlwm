// SPDX-License-Identifier: GPL-2.0-only
#include "../itlwm/hal_iwx/iwx_mld_mac.h"
#include <cassert>
#include <vector>
#include <cstdio>

static iwx_mld_link_v1 configured()
{
    iwx_mld_link_v1 cmd = {};
    iwx_mld_put32(&cmd.phy_id, 2);
    iwx_mld_put32(&cmd.cck_rates, 15);
    iwx_mld_put32(&cmd.ofdm_rates, 21);
    iwx_mld_put32(&cmd.bi, 100);
    iwx_mld_put32(&cmd.dtim, 200);
    iwx_mld_put16(&cmd.ac[0].cw_min, 15);
    cmd.local_addr[0] = 2;
    cmd.local_addr[5] = 0xa5;
    return cmd;
}

int main()
{
    const uint8_t addr[] = {2, 0, 0, 0, 0, 0xa5};
    iwx_mld_mac_v2 mac;
    assert(iwx_mld_build_mac(&mac, 0, 1, 5, addr, false, 0, false));
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(&mac);
    assert(raw[4] == 1 && raw[8] == 5 && raw[12] == 2 && raw[17] == 0xa5);
    assert(raw[20] == 12 && raw[32] == 1 && raw[36] == 0);
    assert(iwx_mld_build_mac(&mac, 0, 2, 5, addr, true, 0xc123, true));
    assert(raw[20] == 4 && raw[24] == 1 && raw[36] == 1);
    assert(raw[40] == 0x23 && raw[41] == 1);
    assert(raw[18] == 0 && raw[19] == 0 && raw[26] == 0 && raw[44] == 0);
    assert(iwx_mld_build_mac(&mac, 0, 3, 5, addr, false, 0, false));
    for (size_t n = 8; n < sizeof(mac); n++) assert(raw[n] == 0);
    assert(!iwx_mld_build_mac(&mac, 4, 1, 5, addr, false, 0, false));
    uint32_t cck, ofdm;
    const uint8_t basic[] = {0x8c, 18, 0x98, 36, 0xb0, 72, 96, 108};
    iwx_mld_basic_rates(basic, sizeof(basic), &cck, &ofdm);
    assert(cck == 15 && ofdm == 0x15);
    const uint8_t all[] = {0x8c, 0x92, 0x98, 0xa4, 0xb0, 0xc8, 0xe0, 0xec};
    iwx_mld_basic_rates(all, sizeof(all), &cck, &ofdm);
    assert(ofdm == 0xff);

    const auto base = configured();
    // A scan needs a firmware link ID before the PHY exists. Precreate that
    // inactive link using only the minimal ADD fields.
    bool present = false, active = false, timing = false;
    std::vector<iwx_mld_link_v1> calls;
    auto send = [&](const iwx_mld_link_v1 &cmd) { calls.push_back(cmd); return 0; };
    assert(iwx_mld_add_link_inactive(base, present, active, timing, send) == 0);
    assert(present && !active && !timing && calls.size() == 1);
    const auto &pre_scan = calls.back();
    assert(iwx_mld_get32(&pre_scan.action) == 1);
    assert(iwx_mld_get32(&pre_scan.link_id) == iwx_mld_get32(&base.link_id));
    assert(iwx_mld_get32(&pre_scan.mac_id) == iwx_mld_get32(&base.mac_id));
    assert(iwx_mld_get32(&pre_scan.phy_id) == UINT32_MAX);
    assert(memcmp(pre_scan.local_addr, base.local_addr, sizeof(base.local_addr)) == 0);
    const uint8_t *pre_scan_bytes = reinterpret_cast<const uint8_t *>(&pre_scan);
    for (size_t n = 0; n < sizeof(pre_scan); n++) {
        if ((n >= offsetof(iwx_mld_link_v1, action) &&
             n < offsetof(iwx_mld_link_v1, action) + sizeof(pre_scan.action)) ||
            (n >= offsetof(iwx_mld_link_v1, link_id) &&
             n < offsetof(iwx_mld_link_v1, link_id) + sizeof(pre_scan.link_id)) ||
            (n >= offsetof(iwx_mld_link_v1, mac_id) &&
             n < offsetof(iwx_mld_link_v1, mac_id) + sizeof(pre_scan.mac_id)) ||
            (n >= offsetof(iwx_mld_link_v1, phy_id) &&
             n < offsetof(iwx_mld_link_v1, phy_id) + sizeof(pre_scan.phy_id)) ||
            (n >= offsetof(iwx_mld_link_v1, local_addr) &&
             n < offsetof(iwx_mld_link_v1, local_addr) + sizeof(pre_scan.local_addr)))
            continue;
        assert(pre_scan_bytes[n] == 0);
    }
    // Repeating pre-scan setup is idempotent, then AUTH fills PHY and activates.
    assert(iwx_mld_add_link_inactive(base, present, active, timing, send) == 0);
    assert(calls.size() == 1 && present && !active);
    assert(iwx_mld_apply_link(base, 1, false, present, active, timing, send) == 0);
    assert(present && active && calls.size() == 3);
    assert(iwx_mld_get32(&calls[1].action) == 2);
    assert(iwx_mld_get32(&calls[1].modify_mask) == 0);
    assert(iwx_mld_get32(&calls[1].active) == 0);
    assert(iwx_mld_get32(&calls[1].phy_id) == 2);
    assert(iwx_mld_get32(&calls[2].action) == 2);
    assert(iwx_mld_get32(&calls[2].modify_mask) == 15);
    assert(iwx_mld_get32(&calls[2].active) == 1);
    assert(iwx_mld_apply_link(base, 1, false, present, active, timing, send) == 0);
    assert(calls.size() == 3 && present && active);
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing,
        [&](const iwx_mld_link_v1 &cmd) {
            calls.push_back(cmd);
            return iwx_mld_get32(&cmd.action) == 3 ? 29 : 0;
        }) == 29);
    assert(present && !active && calls.size() == 5);
    assert(iwx_mld_get32(&calls[3].action) == 2 &&
           iwx_mld_get32(&calls[3].active) == 0);
    assert(iwx_mld_get32(&calls[4].action) == 3);
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing, send) == 0);
    assert(!present && !active && !timing && calls.size() == 6);

    // If AUTH cannot assign a PHY and rollback REMOVE also fails, retain an
    // accurate present/inactive state so the caller can force HW recovery.
    present = active = timing = false;
    calls.clear();
    assert(iwx_mld_add_link_inactive(base, present, active, timing, send) == 0);
    assert(iwx_mld_apply_link(base, 1, false, present, active, timing,
        [&](const iwx_mld_link_v1 &cmd) {
            calls.push_back(cmd);
            return iwx_mld_get32(&cmd.action) == 2 ? 31 : 32;
        }) == 31);
    assert(present && !active && !timing && calls.size() == 3);
    assert(iwx_mld_get32(&calls[1].action) == 2 &&
           iwx_mld_get32(&calls[1].modify_mask) == 0);
    assert(iwx_mld_get32(&calls[2].action) == 3);
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing, send) == 0);
    assert(!present && !active && !timing && calls.size() == 4);

    // Inject failure at each firmware operation of ADD; cleanup retains the
    // original error, and a successful REMOVE releases the host resource.
    for (unsigned fail = 1; fail <= 3; fail++) {
        present = active = timing = false;
        std::vector<iwx_mld_link_v1> calls;
        int err = iwx_mld_apply_link(base, 1, false, present, active, timing,
            [&](const iwx_mld_link_v1 &cmd) {
                calls.push_back(cmd);
                return calls.size() == fail ? 27 : 0;
            });
        assert(err == 27 && !present && !active && !timing);
        assert(iwx_mld_get32(&calls[0].phy_id) == UINT32_MAX);
        if (fail > 1) assert(iwx_mld_get32(&calls.back().action) == 3);
    }
    present = active = timing = false;
    calls.clear();
    assert(iwx_mld_apply_link(base, 1, false, present, active, timing, send) == 0);
    assert(present && active && !timing && calls.size() == 3);
    assert(iwx_mld_get32(&calls[1].modify_mask) == 0);
    assert(iwx_mld_get32(&calls[1].active) == 0);
    assert(iwx_mld_get32(&calls[1].phy_id) == 2);
    assert(iwx_mld_get32(&calls[2].modify_mask) == 15);
    assert(iwx_mld_get32(&calls[2].ofdm_rates) == 21);
    assert(iwx_mld_get32(&calls[2].bi) == 100);
    assert(iwx_mld_apply_link(base, 2, true, present, active, timing, send) == 0);
    assert(timing && (iwx_mld_get32(&calls.back().modify_mask) & IWX_MLD_MODIFY_BEACON));
    assert(iwx_mld_apply_link(base, 2, true, present, active, timing, send) == 0);
    assert(!(iwx_mld_get32(&calls.back().modify_mask) & IWX_MLD_MODIFY_BEACON));
    // A failed deactivation must not remove a still-active link.
    size_t before = calls.size();
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing,
        [&](const iwx_mld_link_v1 &) { return 28; }) == 28);
    assert(present && active && calls.size() == before);
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing, send) == 0);
    assert(!present && !active && !timing && calls.size() == before + 2);
    assert(iwx_mld_get32(&calls.back().action) == 3);
    assert(iwx_mld_get32(&calls.back().phy_id) == UINT32_MAX);
    assert(iwx_mld_apply_link(base, 3, false, present, active, timing, send) == 0);
    assert(calls.size() == before + 2);
    std::puts("MLD MAC wire and link lifecycle/failure tests passed");
}
