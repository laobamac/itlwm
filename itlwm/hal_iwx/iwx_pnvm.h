/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IWX_PNVM_H
#define IWX_PNVM_H

/* Pure parser: no allocations or device access; also used by unit tests. */
#include <stdint.h>
#include <stddef.h>

#define IWX_PNVM_MAX_CHUNKS 64
#define IWX_PNVM_DMA_TABLE_SIZE (IWX_PNVM_MAX_CHUNKS * sizeof(uint64_t))
#define IWX_PNVM_DMA_ALIGNMENT 4096

struct iwx_pnvm_image {
    const uint8_t *chunks[IWX_PNVM_MAX_CHUNKS];
    uint32_t lengths[IWX_PNVM_MAX_CHUNKS];
    uint32_t count, size, version;
};

enum iwx_pnvm_result {
    IWX_PNVM_OK, IWX_PNVM_NOT_FOUND, IWX_PNVM_INVALID
};

/* The driver's compatibility strrchr returns AFTER the delimiter. Find the
 * final -API.ucode boundary directly so this resource key has libc semantics
 * in both the kernel and host tests. Do not change output on invalid input.
 */
static inline bool iwx_pnvm_filename(const char *firmware, char *output, size_t capacity)
{
    if (!firmware || !output || capacity < sizeof(".pnvm"))
        return false;
    const char *suffix = NULL;
    for (const char *p = firmware; *p; p++)
        if (*p == '-') suffix = p;
    if (!suffix || suffix == firmware)
        return false;
    const char *api = suffix + 1;
    if (*api < '0' || *api > '9') return false;
    while (*api >= '0' && *api <= '9') api++;
    const char extension[] = ".ucode";
    for (size_t i = 0; i < sizeof(extension); i++) {
        if (api[i] != extension[i]) return false;
        if (!extension[i]) break;
    }
    size_t prefix = (size_t)(suffix - firmware);
    if (prefix > capacity - sizeof(".pnvm"))
        return false;
    for (size_t i = 0; i < prefix; i++) output[i] = firmware[i];
    const char pnvm[] = ".pnvm";
    for (size_t i = 0; i < sizeof(pnvm); i++) output[prefix + i] = pnvm[i];
    return true;
}

static inline uint32_t iwx_pnvm_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* PNVM_INIT_COMPLETE_NTFY_S_VER_1: Linux records this opaque status and
 * completes the wait regardless of its value. It is not a zero-success errno.
 * A received, bounded notification does not prove calibration/radio health.
 */
static inline bool iwx_pnvm_completion_status(const uint8_t *data, size_t length,
    uint32_t *status)
{
    if (!data || length < sizeof(uint32_t) || !status)
        return false;
    *status = iwx_pnvm_le32(data);
    return true;
}

/* Match Linux's separate coherent allocations: each address starts a page
 * and one payload must not cross a 4 GiB DMA boundary. The unused descriptor
 * slots stay zero. Validate all inputs before changing the table.
 */
static inline bool iwx_pnvm_dma_region_valid(uint64_t address, uint32_t length)
{
    return address && length && !(address & (IWX_PNVM_DMA_ALIGNMENT - 1)) &&
        address <= UINT64_MAX - (length - 1) &&
        (address >> 32) == ((address + length - 1) >> 32);
}

static inline bool iwx_pnvm_build_dma_table(uint8_t *table, size_t capacity,
    const uint64_t *addresses, const uint32_t *lengths, size_t count)
{
    if (!table || capacity < IWX_PNVM_DMA_TABLE_SIZE || !addresses || !lengths ||
        !count || count > IWX_PNVM_MAX_CHUNKS) return false;
    for (size_t i = 0; i < count; i++)
        if (!iwx_pnvm_dma_region_valid(addresses[i], lengths[i])) return false;
    for (size_t i = 0; i < IWX_PNVM_DMA_TABLE_SIZE; i++) table[i] = 0;
    for (size_t i = 0; i < count; i++)
        for (size_t byte = 0; byte < sizeof(uint64_t); byte++)
            table[i * sizeof(uint64_t) + byte] = (uint8_t)(addresses[i] >> (byte * 8));
    return true;
}

/* TLV IDs and selection follow Linux v6.6 drivers/.../iwlwifi/fw/pnvm.c. */
static inline iwx_pnvm_result
iwx_pnvm_parse(const uint8_t *data, size_t len, const uint32_t sku[3],
    uint16_t mac, uint16_t rf, struct iwx_pnvm_image *image)
{
    bool selected = false, hw_match = false;
    *image = {};
    while (len) {
        if (len < 8)
            return IWX_PNVM_INVALID;
        uint32_t type = iwx_pnvm_le32(data);
        uint32_t size = iwx_pnvm_le32(data + 4);
        data += 8;
        len -= 8;
        /* Calculate padding without overflowing a 32-bit length. */
        size_t padding = (4 - (size & 3)) & 3;
        if (size > len || padding > len - size)
            return IWX_PNVM_INVALID;
        if (type == 64) { /* PNVM_SKU */
            if (size != 12)
                return IWX_PNVM_INVALID;
            if (selected && hw_match && image->count)
                return IWX_PNVM_OK;
            selected = iwx_pnvm_le32(data) == sku[0] &&
                iwx_pnvm_le32(data + 4) == sku[1] &&
                iwx_pnvm_le32(data + 8) == sku[2];
            hw_match = false;
            *image = {};
        } else if (selected) {
            switch (type) {
            case 62: /* PNVM_VERSION */
                if (size < 4)
                    return IWX_PNVM_INVALID;
                image->version = iwx_pnvm_le32(data);
                break;
            case 58: /* HW_TYPE: two little-endian u16 values */
                if (size < 4)
                    return IWX_PNVM_INVALID;
                hw_match |= (iwx_pnvm_le32(data) & 0xffff) == mac &&
                    (iwx_pnvm_le32(data) >> 16) == rf;
                break;
            case 19: /* SEC_RT: u32 offset, then original chunk bytes */
                if (size < 4)
                    return IWX_PNVM_INVALID;
                if (iwx_pnvm_le32(data) == 0xddddeeee)
                    break; /* Deprecated separator. */
                if (size == 4 || image->count == IWX_PNVM_MAX_CHUNKS ||
                    size - 4 > UINT32_MAX - image->size)
                    return IWX_PNVM_INVALID;
                image->chunks[image->count] = data + 4;
                image->lengths[image->count++] = size - 4;
                image->size += size - 4;
                break;
            }
        }
        data += size + padding;
        len -= size + padding;
    }
    return selected && hw_match && image->count ?
        IWX_PNVM_OK : IWX_PNVM_NOT_FOUND;
}

#endif
