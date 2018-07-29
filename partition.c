#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "partition.h"

/*
 * Profile a disk.  Expects a full iso with valid 
 * disc id and magic number
 */
struct DiscInfo * profileImage(char *file)
{
    // if file pointer is empty read from stdin
    FILE *f = (file != NULL) ? fopen(file, "rb") : stdin;

    // Do all of our reading in 0x40000 byte blocks
    unsigned char buffer[BLOCK_SIZE];
    size_t read;

    struct DiscInfo *discInfo = malloc(sizeof(struct DiscInfo));
    
    // force our blockNum to be an unsigned 64 bit int (8 bytes * 8 bits)
    // to make copying to the partition table easier
    uint64_t blockNum = 0;
    uint64_t shrunkenBlockNum = 1;
    while((read = fread(buffer, 1, BLOCK_SIZE, f)) > 0) {

        // get the disc info from the first block
        if (blockNum == 0) {
            getDiscInfo(discInfo, buffer);
            blockNum++;
            continue;
        }

        // if the first block has the shrunken magic word
        // the rest of the disc info is in the second block
        if (blockNum == 1 & discInfo->isShrunken) {
            getDiscInfo(discInfo, buffer);
            return discInfo;
        }

        // get the junk block for this block number
        unsigned char * junk = getJunkBlock(blockNum, discInfo->discId, discInfo->discNumber);

        // check if this is a generated junk block
        if (isSame(buffer, junk, read)) {
            // Write a block of FF's to our partition table
            memcpy(discInfo->table + (blockNum * 8), &FFs, 8);
        } 

        // If this is not a junk block then it is a data block
        else {
            // Write the block number to the partition table
            shrunkenBlockNum++;
            memcpy(discInfo->table + (blockNum * 8), &shrunkenBlockNum, 8);
        }
        blockNum++;
    }
    fclose(f);

    discInfo->shrunkenSize = shrunkenBlockNum;

    if (blockNum > 32467) {
        discInfo->isDualLayer = true;
    }

    discInfo->table[7] = discInfo->isGC ? GC_DISC :
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
	// struct DiscInfo *discInfo = malloc(sizeof(struct DiscInfo));

    // check for the shrunken magic word
    bool isShrunken = memcmp(data, SHRUNKEN_MAGIC_WORD, 8) == 0;

    // if this is a shrunken disc image the first block
    // is the partition table and the second block has all the
    // disc info
    if (isShrunken) {

        // create a partition table
        discInfo->table = malloc(sizeof(char) * BLOCK_SIZE);
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
        discInfo->discId = malloc(7);
        memcpy(discInfo->discId, data, 6);
        memset(discInfo->discId + 6, 0, 1);
        
        // the disc number comes from byte 6
        discInfo->discNumber = data[6];

        // the disc name comes at byte 32
        size_t nameLength = strnlen(data + 32, 100);
        discInfo->discName = malloc(sizeof(char) * nameLength + 1);
        memcpy(discInfo->discName, data + 32, nameLength + 1);

        discInfo->isGC = memcmp(data + 28, GC_MAGIC_WORD, 4) == 0;
        discInfo->isWII = memcmp(data + 24, WII_MAGIC_WORD, 4) == 0;

        if (!discInfo->isGC && !discInfo->isWII) {
            fprintf(stderr, "ERROR: We are not a GC or WII disc\n");
            return;
        }

        if (discInfo->table == NULL) {
            // create a partition table
            discInfo->table = malloc(sizeof(char) * BLOCK_SIZE);
            memset(discInfo->table, 0, BLOCK_SIZE);

            // write the shrunken magic word to the partition table
            memcpy(discInfo->table, SHRUNKEN_MAGIC_WORD, 8);

            // set disc info in partition table
            memcpy(discInfo->table + 8, discInfo->discId, 6);

            // set the disc number in the partition table
            memcpy(discInfo->table + 8 + 6, &discInfo->discNumber, 1);
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
        else if (memcmp(&FFs, discInfo->table + (blockNum * 8), 8) == 0) {
            if (dataCount > 0){
                printf("%05d blocks of data\n", dataCount);
                dataCount = 0;
            }
            junkCount++;
        }

        // we are a data block
        else {
            if (junkCount > 0) {
                printf("%05d blocks of junk\n", junkCount);
                junkCount = 0;
            }
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

    printf("%05d TOTAL SHRUNKEN BLOCKS\n", discInfo->shrunkenSize);
}
