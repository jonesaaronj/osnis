#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Calculate a CRC32 checksum on the given buffer and length
 */
uint32_t crc32(const unsigned char *buf, size_t len, uint32_t init);

#endif