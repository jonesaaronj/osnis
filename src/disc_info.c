#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "disc_info.h"
#include "crc32.h"

/*
 * Profile a disk.  Expects a full iso with valid 
 * disc id and magic number
 */
struct DiscInfo * profileImage(char *file)
{
    // if file pointer is empty read from stdin
    FILE *f = (file != NULL) ? fopen(file, "rb") : stdin;

    struct DiscInfo *discInfo = calloc(sizeof(struct DiscInfo), 1);
    
    // Do all of our reading in blocks the size of our sector
    unsigned char * buffer = calloc(1, SECTOR_SIZE);
    unsigned char * junk = NULL;
    unsigned char * repeatByte;
    uint32_t prevCrc = 0;
    uint32_t dataSector = 64;
    size_t sector = 0;
    size_t read;
    while((read = fread(buffer, 1, SECTOR_SIZE, f)) > 0) {

        // get the disc info from the first block
        if (sector == 0) {
            getDiscInfo(discInfo, buffer, sector);
        }

        // if the first block has the shrunken magic word this 
        // is a shrunken image and the first block is the partition
        // table and the disc info will be in the next sectors
        else if (discInfo->isShrunken) {
            if (sector <= discInfo->tableSectors) {
                getDiscInfo(discInfo, buffer, sector);
            } else {
                return discInfo;
            }
        }

        // get the junk block for this sector number
        // for the purposes of getting junk the blockNum starts at 0
        // junk blocks get created at a block size of 0x40000
        if (sector % JUNK_SECTOR_SIZE == 0) {
            junk = getJunkBlock(sector / JUNK_SECTOR_SIZE, discInfo->discId, discInfo->discNumber);
        }

        // check if this is a junk block
        if (memcmp(buffer, junk + (sector * SECTOR_SIZE), read) == 0) {
            // Write ffs to the partition table for the address
            memcpy(discInfo->table + ((sector + 1) * 8), &FFs, 4);

            // get the crc of the junk block and copy it to the table
            uint32_t crc = crc32(junk + (sector * SECTOR_SIZE), read, 0);
            memcpy(discInfo->table + ((sector + 1) * 8) + 4, &crc, 4);
        }

        // check if this is a block of repeated junk byte
        else if((repeatByte = isUniform(buffer, read)) != NULL) {
            // write our repeated byte to the partition table
            memcpy(discInfo->table + ((sector + 1) * 8), &FEs, 4);
            memcpy(discInfo->table + ((sector + 1) * 8) + 7, repeatByte, 1);
        }

        // If this is not a junk block then it is a data block
        else {
            // get the crc of the block
            uint32_t crc = crc32(buffer, read, 0);

            // only advance the block number if this was not a repeat block
            if (prevCrc != crc) {
                dataSector++;
            }
            prevCrc = crc;

            // copy the block number and crc to the table
            memcpy(discInfo->table + ((sector + 1) * 8), &dataSector, 4);
            memcpy(discInfo->table + ((sector + 1) * 8) + 4, &crc, 4);
        }
        sector++;
    }
    fclose(f);

    if (sector == WII_DL_SECTOR_NUM) {
        discInfo->isDualLayer = true;
    }

    // set the disc type
    if (discInfo->isWII && discInfo->isDualLayer && sector == WII_DL_SECTOR_NUM) {
        memset(discInfo->table + 6, WII_DL_SECTOR_PT, 1);
        memset(discInfo->table + 7, WII_DL_DISC, 1);
    } else if(discInfo->isWII && sector == WII_SECTOR_NUM) {
        memset(discInfo->table + 6, WII_DL_SECTOR_PT, 1);
        memset(discInfo->table + 7, WII_DISC, 1);
    } else if(discInfo->isGC && sector == GC_SECTOR_NUM) {
        memset(discInfo->table + 6, GC_SECTOR_PT, 1);
        memset(discInfo->table + 7, GC_DISC, 1);
    } else {
        fprintf(stderr, "ERROR: bad number of sectors read\n");
    }

    return discInfo;
}

void getDiscInfo(struct DiscInfo *discInfo, unsigned char data[], size_t sector)
{
    bool isShrunken = memcmp(SHRUNKEN_MAGIC_WORD, data, 5) == 0;

    if (isShrunken) {

        discInfo->isShrunken = isShrunken;

        // for shrunken images the disc type is at byte 7
        switch(data[7]) {
            case 0x01: // GC_DISC
                discInfo->isGC = true;
                discInfo->tableSectors = GC_SECTOR_PT;
                break;
            case 0x10: // WII_DISC
                discInfo->isWII = true;
                discInfo->tableSectors = WII_SECTOR_PT;
                break;
            case 0x11: // WII_DL_DISC
                discInfo->isWII = true;
                discInfo->isDualLayer = true;
                discInfo->tableSectors = WII_DL_SECTOR_PT;
                break;
        }

        discInfo->table = calloc(1, (SECTOR_SIZE * discInfo->tableSectors));
    } 

    // if this is a shrunken image and the sector is part of the partition table
    // write it to the table
    else if (discInfo->isShrunken && sector < discInfo->tableSectors) {
        memcpy(discInfo->table, data + (sector * SECTOR_SIZE), SECTOR_SIZE);
    } 

    // the actual disk info is either in the first sector of a regular image
    // or the sector after the partition table in a shrunken image
    else if (sector == 0 || sector == discInfo->tableSectors) {

        // the disc id comes from bytes 0 through 5
        discInfo->discId = calloc(1, 7);
        memcpy(discInfo->discId, data, 6);
            
        // the disc number comes from byte 6
        discInfo->discNumber = data[6];

        // the disc name comes at byte 32
        size_t nameLength = strlen((const char *) data + 32);
        discInfo->discName = calloc(1, nameLength + 1);
        memcpy(discInfo->discName, data + 32, nameLength);

        discInfo->isGC = memcmp(GC_MAGIC_WORD, data + 28, 4) == 0;
        discInfo->isWII = memcmp(WII_MAGIC_WORD, data + 24, 4) == 0;

        if (!discInfo->isGC && !discInfo->isWII) {
            fprintf(stderr, "ERROR: This is not a GC or WII image\n");
            return;
        }

        if (discInfo->table == NULL) {
            // create a partition table of the max size
            if (discInfo->isGC) {
                discInfo->table = calloc(1, GC_SECTOR_NUM);
            }
            if (discInfo->isWII) {
                discInfo->table = calloc(1, WII_DL_SECTOR_NUM);
            }
            // write the shrunken magic word to the partition table
            memcpy(discInfo->table, SHRUNKEN_MAGIC_WORD, 6);
        }
    }
}

/**
 * Print out the disc info
 */
void printDiscInfo(struct DiscInfo * discInfo) {

    // by the time we get here we should know if this is a wii or gc image
    if (!discInfo->isGC && !discInfo->isWII) {
        fprintf(stderr, "ERROR: This is not a GC or WII image\n");
        return;
    }

    if (discInfo->isShrunken) fprintf(stderr, "Shrunken ");
    if (discInfo->isDualLayer) fprintf(stderr, "Dual Layer ");
    if (discInfo->isGC) fprintf(stderr, "Gamecube Image Found!!!\n");
    if (discInfo->isWII) fprintf(stderr, "WII Image Found!!!\n");
    fprintf(stderr, "Disc Id: %.*s\n", 6, discInfo->discId);
    fprintf(stderr, "Disc Name: %s\n", discInfo->discName);
    fprintf(stderr, "Disc Number: %d\n", discInfo->discNumber);

    uint32_t prevCrc = 0;

    int dataCount = 0;
    int generatedJunkCount = 0;
    int repeatJunkCount = 0;
    int repeatSector = 0;
    
    int sector;
    for(sector = 1; sector < WII_DL_SECTOR_NUM; sector++) {
        
        // if 8 00s we are at the end of the disc
        if (memcmp(&ZEROs, discInfo->table + (sector * 8), 8) == 0) {
            break;
        }

        // if you see FF address this is a junk block
        else if (memcmp(&FFs, discInfo->table + (sector * 8), 4) == 0) {
            if (dataCount > 0){
                fprintf(stderr, "%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (repeatJunkCount > 0){
                fprintf(stderr, "%05d blocks of repeat\n", repeatJunkCount);
                repeatJunkCount = 0;
            }
            generatedJunkCount++;
        }

        // if you see 00 address this is a repeat block
        else if (memcmp(&ZEROs, discInfo->table + (sector * 8), 4) == 0) {
            if (generatedJunkCount > 0) {
                fprintf(stderr, "%05d blocks of junk\n", generatedJunkCount);
                generatedJunkCount = 0;
            }
            if (dataCount > 0){
                fprintf(stderr, "%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            repeatJunkCount++;
        }

        // we are a data block
        else {
            if (generatedJunkCount > 0) {
                fprintf(stderr, "%05d blocks of generated junk\n", generatedJunkCount);
                generatedJunkCount = 0;
            }
            if (repeatJunkCount > 0){
                fprintf(stderr, "%05d blocks of repeat junk\n", repeatJunkCount);
                repeatJunkCount = 0;
            }

            // see if we are a repeated data block
            if (memcmp(&prevCrc, discInfo->table + (sector * 8) + 4, 4) == 0) {
                repeatSector++;
            }
            memcpy(&prevCrc, discInfo->table + (sector * 8) + 4, 4);

            dataCount++;
        }
    }
    if (dataCount > 0){
        fprintf(stderr, "%05d blocks of data\n", dataCount);
    }
    if (generatedJunkCount > 0) {
        fprintf(stderr, "%05d blocks of generated junk\n", generatedJunkCount);
    }
    if (repeatJunkCount > 0) {
        fprintf(stderr, "%05d blocks of repeat junk\n", repeatJunkCount);
    }
    if (repeatSector > 0) {
        fprintf(stderr, "%05d BLOCKS REPEATED\n", repeatSector);
    }

    fprintf(stderr, "%05d TOTAL SECTORS\n", sector - 1);
}
