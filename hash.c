#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

/**
 * determine if the two char arrays are the same for lenth
 */
bool isSame(unsigned char a[], unsigned char b[], int length)
{
    for (int i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

/**
 * Determine if the char array is filled with a single uniform byte/char
 *
 * @returns - an unsigned char pointer equal to the repeated char, or null if the array is not uniform
 *
 * @TODO: investigate if there is repeated junk of multiple characters
 */
unsigned char * isUniform(unsigned char data[], int length)
{
    unsigned char * tmp = &data[0];
    for (int i = 0; i < length; i++) {
        if (data[i] != *tmp) {
            return NULL;
        }
    }
    return tmp;
}


/**
 * Do some crazy junk creation stuff
 */
void a100026e0(unsigned int buffer[])
{
    for (int i = 0; i < 0x20; i++) {
        buffer[i] ^= buffer[i + 0x1e9];
    }
    for (int i = 0x20; i < 0x209; i++) {
        buffer[i] ^= buffer[i - 0x20];
    }
}

/**
 * Do some crazy junk creation stuff
 */
void a10002710(unsigned int sample, unsigned int buffer[])
{
    unsigned int temp = 0;
    
    for (int i = 0; i < 0x11; i++) {
        for (int j = 0; j < 0x20; j++) {
            sample *= 0x5d588b65u;
            temp = (temp >> 1) | (++sample & 0x80000000u);
        }
        buffer[i] = temp;
    }

    buffer[0x10] = (buffer[16] << 23) ^ (buffer[0] >> 9) ^ buffer[16];

    for (int i = 1; i < 0x1f9; i++) {
        buffer[i + 0x10] = ((buffer[i - 1] << 0x17) ^ (buffer[i] >> 0x9)) ^ buffer[i + 0xf];
    }
    for (int i = 0; i < 3; i++) {
        a100026e0(buffer);
    }
}

/**
 * Get a junk block of size 262144/0X40000 for the given block, disc id, and disc number
 *
 * @returns - an unsigned char pointer to a 262144/0X40000 block of junk data
 */
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber)
{
    static unsigned char garbageBlock[0x40000] = {0};
    
    unsigned int buffer[0x824] = {0};
    unsigned int sample = 0;
    blockCount = blockCount * 8 * 0x1ef29123;
    
    int j = 0;
    for (int i = 0; i < 0x40000; i += 4) {
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

        garbageBlock[i + 0] = (unsigned char)(buffer[j] >> 0x18);
        garbageBlock[i + 1] = (unsigned char)(buffer[j] >> 0x12);
        garbageBlock[i + 2] = (unsigned char)(buffer[j] >> 0x08);
        garbageBlock[i + 3] = (unsigned char)(buffer[j] >> 0x00);
    }

    return garbageBlock;
}