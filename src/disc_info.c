#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "disc_info.h"
#include "crc32.h"

void getDiscInfo(struct DiscInfo *discInfo, unsigned char data[], size_t sector)
{
    discInfo->isShrunken |= memcmp(SHRUNKEN_MAGIC_WORD, data, 5) == 0;

    // if the first sector has the shrunken keyword
    if (sector == 0 && discInfo->isShrunken) {

        // for shrunken images the disc type is at byte 7
        switch(data[7]) {
            case 0x01: // GC_DISC
                discInfo->isGC = true;
                discInfo->sectors = GC_SECTOR_NUM;
                break;
            case 0x10: // WII_DISC
                discInfo->isWII = true;
                discInfo->sectors = WII_SECTOR_NUM;
                break;
            case 0x11: // WII_DL_DISC
                discInfo->isWII = true;
                discInfo->isDualLayer = true;
                discInfo->sectors = WII_DL_SECTOR_NUM;
                break;
        }

        discInfo->tableSectors = data[6];

        discInfo->table = calloc(1, (SECTOR_SIZE * discInfo->tableSectors));
        memcpy(discInfo->table, data, SECTOR_SIZE);
    }

    // if this is a shrunken image and the sector is part of the partition table
    // write it to the table
    else if (discInfo->isShrunken && 0 < sector && sector < discInfo->tableSectors) {
        memcpy(discInfo->table + (sector * SECTOR_SIZE), data, SECTOR_SIZE);
    } 

    // if this is not a shrunken image the key issuer will be in sector 0x0A at byte 0x140
    else if (!discInfo->isShrunken && discInfo->isWII && sector == 0x0A) {
        size_t issuerLength = strlen((const char *) data + 0x140);
        discInfo->issuer = calloc(1, issuerLength + 1);
        memcpy(discInfo->issuer, data + 0x140, issuerLength);
    }

    // the actual disk info is either in the first sector of a regular image
    // or the sector after the partition table in a shrunken image
    else if ((!discInfo->isShrunken && sector == 0) || (discInfo->isShrunken && sector == discInfo->tableSectors)) {

        // the disc id comes from bytes 0 through 5
        discInfo->discId = calloc(1, 6);
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
                discInfo->tableSectors = GC_SECTOR_PT;
                discInfo->table = calloc(1, GC_SECTOR_NUM * 8);
            }
            if (discInfo->isWII) {
                discInfo->tableSectors = WII_DL_SECTOR_PT;
                discInfo->table = calloc(1, WII_DL_SECTOR_NUM * 8);
            }
            // write the shrunken magic word to the partition table
            memcpy(discInfo->table, SHRUNKEN_MAGIC_WORD, 6);
        }
    }
}

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
    // unsigned char * empty = calloc(1, SECTOR_SIZE);
    unsigned char * buffer = calloc(1, SECTOR_SIZE);
    unsigned char * junk = calloc(1, JUNK_BLOCK_SIZE);
    unsigned char * repeatByte;
    uint32_t prevCrc = 0;
    uint32_t dataSector = -1;
    size_t sector = 0;
    size_t read;
    while((read = fread(buffer, 1, SECTOR_SIZE, f)) == SECTOR_SIZE) {

        getDiscInfo(discInfo, buffer, sector);
        
        // get the disc info from the first block
        if (sector == 0 && discInfo->isShrunken) {
            sector++;
            continue;
        }

        // if the first block has the shrunken magic word this 
        // is a shrunken image and the first block is the partition
        // table and the disc info will be in the next few sectors
        else if (discInfo->isShrunken) {
            if (sector <= discInfo->tableSectors) {
                sector++;
                continue;
            } else {
                return discInfo;
            }
        }

        // get the junk block for this sector number
        // for the purposes of getting junk the blockNum starts at 0
        // junk blocks get created at a block size of 0x40000
        // if (sector % JUNK_SECTOR_SIZE == 0) {
        getJunkBlock(junk, sector / JUNK_SECTOR_SIZE, discInfo->discId, discInfo->discNumber);
        // int same = compare(buffer, junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), SECTOR_SIZE);
        // if (SECTOR_SIZE/10 < same && same < SECTOR_SIZE) fprintf(stderr, "%ld JUNK SAME: %d\n", sector, same);
        //fprintf(stderr, "%ld JUNK SAME: %f\n", sector, same);

        // same = compare(buffer, empty, SECTOR_SIZE);
        // if (SECTOR_SIZE/10 < same && same < SECTOR_SIZE) fprintf(stderr, "%ld EMPTY SAME: %d\n", sector, same);
        //fprintf(stderr, "%ld EMPTY SAME: %f\n", sector, same);
        //}

        // check if this is a junk block
        if (memcmp(buffer, junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), read) == 0) {
            // Write ffs to the partition table for the address
            memcpy(discInfo->table + ((sector + 1) * 8), &FFs, 4);

            // get the crc of the junk block and copy it to the table
            uint32_t crc = crc32(junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), read, 0);
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
        discInfo->tableSectors = WII_DL_SECTOR_PT;
        discInfo->sectors = WII_DL_SECTOR_NUM;
    } else if(discInfo->isWII && sector == WII_SECTOR_NUM) {
        memset(discInfo->table + 6, WII_SECTOR_NUM, 1);
        memset(discInfo->table + 7, WII_DISC, 1);
        discInfo->tableSectors = WII_SECTOR_PT;
        discInfo->sectors = WII_SECTOR_NUM;
    } else if(discInfo->isGC && sector == GC_SECTOR_NUM) {
        memset(discInfo->table + 6, GC_SECTOR_PT, 1);
        memset(discInfo->table + 7, GC_DISC, 1);
        discInfo->tableSectors = GC_SECTOR_PT;
        discInfo->sectors = GC_SECTOR_NUM;
    } else {
        fprintf(stderr, "ERROR: bad number of sectors read\n");
    }

    return discInfo;
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
    if (discInfo->isWII) fprintf(stderr, "Disc Issuer: %s\n", discInfo->issuer);

    uint32_t prevCrc = 0;

    int dataCount = 0;
    int generatedJunkCount = 0;
    int repeatJunkCount = 0;
    int repeatSector = 0;
    
    fprintf(stderr, "Sectors %ld\n", discInfo->sectors);

    int sector;
    for(sector = 1; sector < discInfo->sectors; sector++) {
        
        // if 8 00s we are at the end of the disc
        if (memcmp(&ZEROs, discInfo->table + (sector * 8), 8) == 0) {
            fprintf(stderr, "Shouldn't ever get here\n");
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

        // if you see 0xFE address this is a repeat block
        else if (memcmp(&FEs, discInfo->table + (sector * 8), 4) == 0) {
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
        fprintf(stderr, "%05d blocks of repeated data\n", repeatSector);
    }

    fprintf(stderr, "%05d Total sectors\n", sector);
}
