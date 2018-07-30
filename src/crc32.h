#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stdlib.h>

uint32_t crc_32(const unsigned char *buf, size_t len, uint32_t init);

#endif