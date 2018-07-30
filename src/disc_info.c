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

    // force our blockNum to be an unsigned 64 bit int (8 bytes * 8 bits)
    // to make copying to the partition table easier
    uint64_t blockNum = 0;
    uint32_t dataBlockNum = 1;
    size_t read;
    while((read = fread(buffer, 1, BLOCK_SIZE, f)) > 0) {

        if((repeatByte = isUniform(buffer, read)) != NULL) {
            fprintf(stderr, "Saw a repeat char: %x\n", *repeatByte);
        }

        
        // get the disc info from the first block
        if (blockNum == 0) {
            getDiscInfo(discInfo, buffer);
            blockNum++;
            dataBlockNum++;
            continue;
        }

        // if the first block has the shrunken magic word this 
        // is a shrunken image and the first block is the partition
        // table and the disc info will be in the second block
        if (blockNum == 1 && discInfo->isShrunken) {
            getDiscInfo(discInfo, buffer);
            return discInfo;
        }

        // get the junk block for this block number
        unsigned char * junk = getJunkBlock(blockNum, discInfo->discId, discInfo->discNumber);

        // check if this is a junk block
        if (isSame(buffer, junk, read)) {
            // Write the junk block magic word to our partition table
            memcpy(discInfo->table + ((blockNum + 1)* 8), &JUNK_BLOCK_MAGIC_WORD, 8);
        }

        // If this is not a junk block then it is a data block
        else {
            // copy the block number to the table
            memcpy(discInfo->table + ((blockNum + 1) * 8), &dataBlockNum, 4);
            dataBlockNum++;

            // copy the crc32 to the table
            uint32_t crc_32 = crc32(buffer, read, 0);
            memcpy(discInfo->table + ((blockNum + 1) * 8) + 4, &crc_32, 4);
        }
        blockNum++;
    }
    fclose(f);

    discInfo->shrunkenSize = dataBlockNum - 1;

    if (blockNum + 1 == WII_DL_BLOCK_NUM) {
        discInfo->isDualLayer = true;
    }

    discInfo->table[8 + 7] = discInfo->isGC ? GC_DISC :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_DISC : WII_DISC;

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
            discInfo->table = calloc(BLOCK_SIZE, 1);

            // write the shrunken magic word to the partition table
            memcpy(discInfo->table, SHRUNKEN_MAGIC_WORD, 9);
        }
    }
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
    if (discInfo->isGC) printf("Gamecube Image Found!!!\n");
    if (discInfo->isWII) printf("WII Image Found!!!\n");
    printf("Disc Id: %.*s\n", 6, discInfo->discId);
    printf("Disc Name: %s\n", discInfo->discName);
    printf("Disc Number: %d\n", discInfo->discNumber);

    int dataCount = 0;
    int junkCount = 0;
    
    int blockNum;
    for(blockNum = 1; blockNum < BLOCK_SIZE; blockNum++) {
        
        // if 8 00s we are at the end of the disc
        if (memcmp(&ZERO, discInfo->table + (blockNum * 8), 8) == 0) {
            break;
        }

        // if 8 FFs we are a junk block
        else if (memcmp(&JUNK_BLOCK_MAGIC_WORD, discInfo->table + (blockNum * 8), 8) == 0) {
            if (dataCount > 0){
                printf("%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            // printf("junk block at %d\n", blockNum);
            junkCount++;
        }

        // we are a data block
        else {
            if (junkCount > 0) {
                printf("%05d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            // printf("data block at %d\n", blockNum);
            dataCount++;
        }
    }
    if (dataCount > 0){
        printf("%05d blocks of data\n", dataCount);
        dataCount = 0;
    }
    if (junkCount > 0) {
        printf("%05d blocks of junk\n", junkCount);
        junkCount = 0;
    }

    printf("%05d TOTAL BLOCKS\n", blockNum - 1);

    printf("%05ld TOTAL SHRUNKEN BLOCKS\n", discInfo->shrunkenSize);
}
