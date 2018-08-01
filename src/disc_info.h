#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>
#include <stdint.h>

static const uint64_t FFs = 0xFFFFFFFFFFFFFFFF;
static const uint64_t FEs = 0xFEFEFEFEFEFEFEFE;
static const uint64_t ZEROs = 0x0;

static const unsigned char GC_DISC = 1;
static const unsigned char WII_DISC = 2;
static const unsigned char WII_DL_DISC = 3;

static const size_t GC_LAST_BLOCK_SIZE = 0x18000;
static const size_t WII_LAST_BLOCK_SIZE = 0x40000;
static const size_t WII_DL_LAST_BLOCK_SIZE = 0x20000;

static const size_t GC_BLOCK_NUM = 5570;
static const size_t WII_BLOCK_NUM = 17929;
static const size_t WII_DL_BLOCK_NUM = 32468;

static const unsigned char SHRUNKEN_MAGIC_WORD[8] = {'O','S','N','I','S',0x00,0x00};

static const unsigned char GC_MAGIC_WORD[] = {0xC2, 0x33, 0x9F, 0x3D};
static const unsigned char WII_MAGIC_WORD[] = {0x5D, 0x1C, 0x9E, 0xA3};

struct DiscInfo
{
    unsigned char * discId;
    unsigned char discNumber;
    unsigned char * discName;
    unsigned char * table;
    bool isGC;
    bool isWII;
    bool isDualLayer;
    bool isShrunken;
};

/**
 * Get disc info from image
 */
struct DiscInfo * profileImage(char *file);

/**
 * Get the disc info from the first block of data
 *
 * If this is a shrunken image call this function twice
 * with the first and second block
 */
void getDiscInfo(struct DiscInfo *discInfo, unsigned char data[]);

/**
 * Print out the disc info
 */
void printDiscInfo(struct DiscInfo * discInfo);

#endif
