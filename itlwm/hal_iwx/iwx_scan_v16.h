/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef IWX_SCAN_V16_H
#define IWX_SCAN_V16_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * API 16 uses the v14 UMAC request wire layout for the supported 2.4/5 GHz
 * scan path. Keep the common 8-byte prefix explicit here so host tests can
 * check real serialized bytes without importing kernel/IOKit headers.
 * The complete nested request layout remains the existing packed
 * iwx_scan_req_umac_v14 in if_iwxreg.h.
 */
namespace iwx_scan_v16 {
static const size_t kPrefixSize = 8;
static const size_t kUidOffset = 0;
static const size_t kOocPriorityOffset = 4;
/* Fixed 2.4/5 GHz v14+ v6 channel-array and v4 probe block sizes. */
static const size_t kGeneralParamsOffset = kPrefixSize;
static const size_t kChannelParamsOffset = kGeneralParamsOffset + 36;
static const size_t kPeriodicParamsOffset = kChannelParamsOffset + 540;
static const size_t kProbeParamsOffset = kPeriodicParamsOffset + 12;
static const size_t kRequestSize = kProbeParamsOffset + 1344;
static const size_t kGeneralReservedOffset = kGeneralParamsOffset + 2;
static const size_t kGeneralReserved1Offset = kGeneralParamsOffset + 9;
static const size_t kPeriodicReservedOffset = kPeriodicParamsOffset + 10;
static const size_t kProbeReservedOffset = kProbeParamsOffset + 532 + 2;

/* Build the common request envelope in an already-sized serialized buffer. */
static inline bool initialize_request(uint8_t *bytes, size_t length,
                                      uint32_t ooc_priority)
{
    if (bytes == NULL || length < kPrefixSize)
        return false;
    memset(bytes, 0, length);
    bytes[kOocPriorityOffset + 0] = (uint8_t)(ooc_priority);
    bytes[kOocPriorityOffset + 1] = (uint8_t)(ooc_priority >> 8);
    bytes[kOocPriorityOffset + 2] = (uint8_t)(ooc_priority >> 16);
    bytes[kOocPriorityOffset + 3] = (uint8_t)(ooc_priority >> 24);
    return true;
}
}

#endif /* IWX_SCAN_V16_H */
