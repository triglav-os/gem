/*
 * Checks disk-sector arithmetic and the local SoftFloat boundary-shift
 * patch with values that previously wrapped or invoked undefined behavior.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
#include "fat12/fat12.h"
#include "m68kcpu.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8_t data[512], output[512];
    fat12_memory_disk_t disk;
    bits64 high, low;
    memset(data, 0x5a, sizeof(data));
    fat12_memory_disk_init(&disk, data, sizeof(data));
    assert(fat12_memory_disk_read_sector(&disk, 0, output, sizeof(output)));
    assert(memcmp(data, output, sizeof(data)) == 0);
    assert(!fat12_memory_disk_read_sector(&disk, 0x800000u, output,
                                          sizeof(output)));
    assert(!fat12_memory_disk_read_sector(&disk, UINT32_MAX, output,
                                          sizeof(output)));
    for (int count = 0; count <= 128; ++count) {
        bits64 expected_high = 0x123456789abcdef0ULL;
        bits64 expected_low = 0xfedcba9876543210ULL;
        for (int bit = 0; bit < count; ++bit) {
            expected_high = (expected_high << 1) | (expected_low >> 63);
            expected_low <<= 1;
        }
        shortShift128Left(0x123456789abcdef0ULL, 0xfedcba9876543210ULL, count,
                          &high, &low);
        assert(high == expected_high && low == expected_low);
    }
    return 0;
}
