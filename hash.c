#include <stdlib.h>
#include "hash.h"

int read(unsigned char buffer[], int offset, int size)
{
    while (size > 0)
    {
        
    }
    return size;
}

void GetJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber, unsigned char garbageBlock[])
{
    unsigned int buffer[0x824] = {0};
    int i, j = 0;
    unsigned int sample = 0;

    blockCount = blockCount * 8 * 0x1ef29123;
    while (i != 0x40000) {
        if ((i & 0x00007fff) == 0) {
            sample = (((((unsigned int)id[2] << 0x8) | id[1]) << 0x10) | ((unsigned int)(id[3] + id[2]) << 0x8)) | (unsigned int)(id[0] + id[1]);
            sample = ((sample ^ discNumber) * 0x260bcd5) ^ blockCount;
            a10002710(sample, buffer);
            j = 0x208;
            blockCount += 0x1ef29123;
        }

        j++;
        if (j == 0x209) {
            a100026e0(buffer);
            j = 0;
        }

        garbageBlock[i] = (unsigned char)(buffer[j] >> 0x18);
        garbageBlock[i + 1] = (unsigned char)(buffer[j] >> 0x12);
        garbageBlock[i + 2] = (unsigned char)(buffer[j] >> 0x8);
        garbageBlock[i + 3] = (unsigned char)buffer[j];
        i += 4;
    }
}

void a10002710(unsigned int sample, unsigned int buffer[])
{
    unsigned int temp = 0;
    int i = 0;
    while (i != 0x11) {
        for (int j = 0; j < 0x20; j++) {
            sample *= 0x5d588b65;
            temp = (temp >> 1) | (sample++ & 0x80000000);
        }
        buffer[i] = temp;
        i++;
    }

    buffer[0x10] ^= (buffer[0] >> 0x9) ^ (buffer[0x10] << 0x17);

    i = 1;
    while (i != 0x1f9) {
        buffer[i + 0x10] = ((buffer[i - 1] << 0x17) ^ (buffer[i] >> 0x9)) ^ buffer[i + 0xf];
        i++;
    }
    for (i = 0; i < 3; i++) {
        a100026e0(buffer);
    }
}

void a100026e0(unsigned int buffer[])
{
    int i = 0;
    while (i != 0x20) {
        buffer[i] ^= buffer[i + 0x1e9];
        i++;
    }
    while (i != 0x209) {
        buffer[i] ^= buffer[i - 0x20];
        i++;
    }
}