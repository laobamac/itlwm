#include "../itlwm/hal_iwx/iwx_scan_v16.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

int main()
{
    /* Exercise serialized bytes at the offsets used by the production UMAC
     * prefix, with a larger body representing the v14/v16 request payload. */
    std::vector<uint8_t> wire(iwx_scan_v16::kRequestSize, 0xa5);
    assert(iwx_scan_v16::initialize_request(wire.data(), wire.size(), 6));
    assert(wire[iwx_scan_v16::kUidOffset + 0] == 0 &&
           wire[iwx_scan_v16::kUidOffset + 1] == 0 &&
           wire[iwx_scan_v16::kUidOffset + 2] == 0 &&
           wire[iwx_scan_v16::kUidOffset + 3] == 0);
    assert(wire[iwx_scan_v16::kOocPriorityOffset + 0] == 6 &&
           wire[iwx_scan_v16::kOocPriorityOffset + 1] == 0 &&
           wire[iwx_scan_v16::kOocPriorityOffset + 2] == 0 &&
           wire[iwx_scan_v16::kOocPriorityOffset + 3] == 0);
    for (size_t i = iwx_scan_v16::kPrefixSize; i < wire.size(); ++i)
        assert(wire[i] == 0); // full body, including all reserved fields, starts zeroed
    assert(wire[iwx_scan_v16::kGeneralReservedOffset] == 0);
    assert(wire[iwx_scan_v16::kGeneralReserved1Offset] == 0);
    assert(wire[iwx_scan_v16::kPeriodicReservedOffset] == 0);
    assert(wire[iwx_scan_v16::kProbeReservedOffset] == 0);

    uint8_t tooShort[7] = {1, 2, 3, 4, 5, 6, 7};
    assert(!iwx_scan_v16::initialize_request(tooShort, sizeof(tooShort), 0));
    assert(tooShort[0] == 1 && tooShort[6] == 7);
    assert(!iwx_scan_v16::initialize_request(NULL, 8, 0));
}
