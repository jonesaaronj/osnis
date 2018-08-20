#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>
#include <stdint.h>

#define SECTOR_SIZE 0x8000

// 00-07 'O','S','N','I','S',0xXX,0xYY,0xZZ
// 0xXX is a version number, I'm starting at 0 for development and when there is a viable working algorithm I'll up it to 1
// 0xYY is image type where 0x01 = GC, 0x10 = WII, and 0x11 is a Dual Layer WII
// 0xZZ is the number of sectors of the partition table
static const unsigned char SHRUNKEN_MAGIC_WORD[8] = {'O','S','N','I','S',0x00,0x00,0x00};

static const unsigned char GC_MAGIC_WORD[] = {0xC2, 0x33, 0x9F, 0x3D};
static const unsigned char WII_MAGIC_WORD[] = {0x5D, 0x1C, 0x9E, 0xA3};

static const uint64_t FFs = 0xFFFFFFFFFFFFFFFF;
static const uint64_t FEs = 0xFEFEFEFEFEFEFEFE;
static const uint64_t ZEROs = 0x0;

static const unsigned char GC_DISC = 0x01;
static const unsigned char GC_SECTOR_PT = 11;
static const size_t GC_SECTOR_NUM = 44555;

static const unsigned char WII_DISC = 0x10;
static const unsigned char WII_SECTOR_PT = 36;
static const size_t WII_SECTOR_NUM = 143432;

static const unsigned char WII_DL_DISC = 0x11;
static const unsigned char WII_DL_SECTOR_PT = 64;
static const size_t WII_DL_SECTOR_NUM = 259740;

struct DiscInfo
{
    unsigned char * discId;
    unsigned char discNumber;
    unsigned char * discName;
    unsigned char * table;
    unsigned char tableSectors;
    size_t sectors;
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
 * with the first and second block to fill the disc info
 */
void getDiscInfo(struct DiscInfo *discInfo, unsigned char data[], size_t sector);

/**
 * Print out the disc info
 */
void printDiscInfo(struct DiscInfo * discInfo);

#endif
