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
    
    // Do all of our reading in 0x40000 byte blocks
    unsigned char * buffer = calloc(1, BLOCK_SIZE);
    unsigned char * repeatByte;
    uint32_t prevCrc = 0;
    uint32_t dataBlockNum = 1;
    size_t blockNum = 0;
    size_t read;
    while((read = fread(buffer, 1, BLOCK_SIZE, f)) > 0) {

        // get the disc info from the first block
        if (blockNum == 0) {
            getDiscInfo(discInfo, buffer);
        }

        // if the first block has the shrunken magic word this 
        // is a shrunken image and the first block is the partition
        // table and the disc info will be in the second block
        else if (blockNum == 1 && discInfo->isShrunken) {
            getDiscInfo(discInfo, buffer);
            return discInfo;
        }

        // get the junk block for this block number
        unsigned char * junk = getJunkBlock(blockNum, discInfo->discId, discInfo->discNumber);

        // check if this is a junk block
        if (isSame(buffer, junk, read)) {
            // Write ffs to the partition table for the address
            memcpy(discInfo->table + ((blockNum + 1) * 8), &FFs, 4);

            // get the crc of the junk block and copy it to the table
            uint32_t crc = crc32(junk, read, 0);
            memcpy(discInfo->table + ((blockNum + 1) * 8) + 4, &crc, 4);
        }

        // check if this is a block of repeated junk byte
        else if((repeatByte = isUniform(buffer, read)) != NULL) {
            // write our repeated byte to the partition table
            memcpy(discInfo->table + ((blockNum + 1) * 8), &ZEROs, 7);
            memcpy(discInfo->table + ((blockNum + 1)* 8) + 7, repeatByte, 1);
        }

        // If this is not a junk block then it is a data block
        else {
            // copy the block number to the table
            memcpy(discInfo->table + ((blockNum + 1) * 8), &dataBlockNum, 4);

            // get the crc of the block and copy it to the table
            uint32_t crc = crc32(buffer, read, 0);
            memcpy(discInfo->table + ((blockNum + 1) * 8) + 4, &crc, 4);

            // only advance the block number if this was not a repeat block
            if (prevCrc != crc) {
                dataBlockNum++;
            }
            prevCrc = crc;
        }
        blockNum++;
    }
    fclose(f);

    discInfo->shrunkenSize = dataBlockNum - 1;

    if (blockNum + 1 == WII_DL_BLOCK_NUM) {
        discInfo->isDualLayer = true;
    }

    return discInfo;
}

/**
 * Get the disc info from the first block of data
 *
 * If this is a shrunken image call this function twice
 * with the first and second block
 */
void getDiscInfo(struct DiscInfo *discInfo, unsigned char data[])
{
	// check for the shrunken magic word
    bool isShrunken = memcmp(SHRUNKEN_MAGIC_WORD, data, 8) == 0;

    // if this is a shrunken disc image the first block
    // is the partition table and the second block has all the
    // disc info
    if (isShrunken) {

        // create a partition table using the data
        discInfo->table = calloc(1, BLOCK_SIZE);
        memcpy(discInfo->table, data, BLOCK_SIZE);
        discInfo->isShrunken = true;

        // for shrunken images the disc type is at byte 16
        switch(data[15]) {
            case 1: // GC_DISC
                discInfo->isGC = true;
                break;
            case 2: // WII_DISC
                discInfo->isWII = true;
                break;
            case 3: // WII_DL_DISC
                discInfo->isWII = true;
                discInfo->isDualLayer = true;
                break;
        }

    } else {
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
            fprintf(stderr, "ERROR: We are not a GC or WII disc\n");
            return;
        }

        if (discInfo->table == NULL) {
            // create a partition table
            discInfo->table = calloc(1, BLOCK_SIZE);

            // write the shrunken magic word to the partition table
            memcpy(discInfo->table, SHRUNKEN_MAGIC_WORD, 8);
        }
    }

    discInfo->discBlockNum = discInfo->isGC ? GC_BLOCK_NUM :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_BLOCK_NUM : WII_BLOCK_NUM;

    discInfo->lastBlockSize = discInfo->isGC ? GC_LAST_BLOCK_SIZE :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_LAST_BLOCK_SIZE : WII_LAST_BLOCK_SIZE;
}

/**
 * Print out the disc info
 */
void printDiscInfo(struct DiscInfo * discInfo) {

    // by the time we get here we should know if this is a wii or gc image
    if (!discInfo->isGC && !discInfo->isWII) {
        fprintf(stderr, "ERROR: We are not a GC or WII disc\n");
        return;
    }

    if (discInfo->isShrunken) printf("Shrunken ");
    if (discInfo->isDualLayer) printf("Dual Layer ");
    if (discInfo->isGC) printf("Gamecube Image Found!!!\n");
    if (discInfo->isWII) printf("WII Image Found!!!\n");
    printf("Disc Id: %.*s\n", 6, discInfo->discId);
    printf("Disc Name: %s\n", discInfo->discName);
    printf("Disc Number: %d\n", discInfo->discNumber);

    uint32_t prevCrc = 0;

    int dataCount = 0;
    int generatedJunkCount = 0;
    int repeatJunkCount = 0;
    int repeatBlock = 0;
    
    int blockNum;
    for(blockNum = 1; blockNum < BLOCK_SIZE; blockNum++) {
        
        // if 8 00s we are at the end of the disc
        if (memcmp(&ZEROs, discInfo->table + (blockNum * 8), 8) == 0) {
            break;
        }

        // if you see FF address this is a junk block
        else if (memcmp(&FFs, discInfo->table + (blockNum * 8), 4) == 0) {
            if (dataCount > 0){
                printf("%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (repeatJunkCount > 0){
                printf("%05d blocks of repeat\n", repeatJunkCount);
                repeatJunkCount = 0;
            }
            generatedJunkCount++;
        }

        // if you see 00 address this is a repeat block
        else if (memcmp(&ZEROs, discInfo->table + (blockNum * 8), 4) == 0) {
            if (generatedJunkCount > 0) {
                printf("%05d blocks of junk\n", generatedJunkCount);
                generatedJunkCount = 0;
            }
            if (dataCount > 0){
                printf("%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            repeatJunkCount++;
        }

        // we are a data block
        else {
            if (generatedJunkCount > 0) {
                printf("%05d blocks of generated junk\n", generatedJunkCount);
                generatedJunkCount = 0;
            }
            if (repeatJunkCount > 0){
                printf("%05d blocks of repeat junk\n", repeatJunkCount);
                repeatJunkCount = 0;
            }

            // see if we are a repeated data block
            if (memcmp(&prevCrc, discInfo->table + (blockNum * 8) + 4, 4) == 0) {
                repeatBlock++;
            }
            memcpy(&prevCrc, discInfo->table + (blockNum * 8) + 4, 4);

            dataCount++;
        }
    }
    if (dataCount > 0){
        printf("%05d blocks of data\n", dataCount);
    }
    if (generatedJunkCount > 0) {
        printf("%05d blocks of generated junk\n", generatedJunkCount);
    }
    if (repeatJunkCount > 0) {
        printf("%05d blocks of repeat junk\n", repeatJunkCount);
    }
    if (repeatBlock > 0) {
        printf("%05d BLOCKS REPEATED\n", repeatBlock);
    }

    printf("%05d TOTAL BLOCKS\n", blockNum - 1);

    printf("%05ld TOTAL SHRUNKEN BLOCKS\n", discInfo->shrunkenSize);

    int n = 5408;
    unsigned char * junk = getJunkBlock(n, discInfo->discId, discInfo->discNumber);
    printf("Junk1 at %d is ", n);
    printChar(junk, 20);
    printf("\n");
}
