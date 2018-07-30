#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hash.h"
#include "disc_info.h"
#include "crc32.h"

/**
 * Unshrink a shrunken image
 */
void unshrinkImage(char *inputFile, char *outputFile) {
    // if file pointer is empty read from stdin
    FILE *inputF = (inputFile != NULL) ? fopen(inputFile, "rb") : stdin;
    // if file pointer is empty write to stdout
    FILE *outputF = (outputFile != NULL) ? fopen(outputFile, "wb") : stdout;

    struct DiscInfo *discInfo = calloc(sizeof(struct DiscInfo), 1);
    
    // Do all of our reading in 0x40000 byte blocks
    unsigned char * buffer = calloc(1, BLOCK_SIZE);

    // get the partition table from the first block
    if (fread(buffer, BLOCK_SIZE, 1, inputF) != 1){
        fprintf(stderr, "ERROR: could not read partition table\n");
        return;
    }
    getDiscInfo(discInfo, buffer);

    // get the first block of our data
    if (fread(buffer, BLOCK_SIZE, 1, inputF) != 1){
        fprintf(stderr, "ERROR: could not read block\n");
        return;
    }
    getDiscInfo(discInfo, buffer);

    size_t discBlockNum = discInfo->isGC ? GC_BLOCK_NUM :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_BLOCK_NUM : WII_BLOCK_NUM;

    size_t lastBlockSize = discInfo->isGC ? GC_LAST_BLOCK_SIZE :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_LAST_BLOCK_SIZE : WII_LAST_BLOCK_SIZE;

    for(int blockNum = 2; blockNum < discBlockNum; blockNum++) {
        
        // set the block size to write
        size_t writeSize = (blockNum == discBlockNum) ? lastBlockSize : BLOCK_SIZE;

        // if 8 00s we are at the end of the disc
        if (memcmp(&ZERO, discInfo->table + (blockNum * 8), 8) == 0) {
            break;
        }

        // if 8 FFs we are a junk block
        else if (memcmp(&JUNK_BLOCK_MAGIC_WORD, discInfo->table + (blockNum * 8), 8) == 0) {
            // get the junk block
            unsigned char * junk = getJunkBlock(blockNum, discInfo->discId, discInfo->discNumber);
            if (fwrite(junk, writeSize, 1, outputF) != 1) {
                fprintf(stderr, "ERROR: could not write block\n");
                break;
            }
        }

        // otherwise we are a data block
        else {
            size_t read;
            // read the data block
            if ((read = fread(buffer, 1, BLOCK_SIZE, inputF)) != BLOCK_SIZE){
                fprintf(stderr, "ERROR: could not read block\n");
                return;
            }
            // check the crc32 of the data block and write if everthing is fine
            uint32_t crc_32 = crc32(buffer, read, 0);
            if (memcmp(&crc_32, discInfo->table + (blockNum * 8) + 4, 4) == 0) {
                if (fwrite(buffer, writeSize, 1, outputF) != 1) {
                    fprintf(stderr, "ERROR: could not write block\n");
                    break;
                }
            }
        }

    }
    fclose(inputF);
    fclose(outputF);
}

/**
 * Create a shrunken image from the input file and disc info
 */
void shrinkImage(struct DiscInfo * discInfo, char *inputFile, char *outputFile) {

    if (!discInfo->isGC && !discInfo->isWII) {
        fprintf(stderr, "ERROR: We are not a GC or WII disc\n");
        return;
    }

    size_t discBlockNum = discInfo->isGC ? GC_BLOCK_NUM :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_BLOCK_NUM : WII_BLOCK_NUM;

    size_t lastBlockSize = discInfo->isGC ? GC_LAST_BLOCK_SIZE :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_LAST_BLOCK_SIZE : WII_LAST_BLOCK_SIZE;

    // if file pointer is empty read from stdin
    FILE *inputF = (inputFile != NULL) ? fopen(inputFile, "rb") : stdin;
    // if file pointer is empty read from stdout
    FILE *outputF = (outputFile != NULL) ? fopen(outputFile, "wb") : stdout;

    // Do all of our reading in 0x40000 byte blocks
    unsigned char * buffer = calloc(1, BLOCK_SIZE);
    
    // write partition block
    if (fwrite(discInfo->table, BLOCK_SIZE, 1, outputF) != 1) {
        fprintf(stderr, "ERROR: could not write block\n");
        return;
    }

    uint32_t dataBlockNum = 1;
    size_t blockNum = 0;
    size_t read;
    while((read = fread(buffer, 1, BLOCK_SIZE, inputF)) > 0) {

        // set the block size to write
        size_t writeSize = (blockNum == discBlockNum - 1) ? lastBlockSize : BLOCK_SIZE;

        // get the junk block
        unsigned char * junk = getJunkBlock(blockNum, discInfo->discId, discInfo->discNumber);

        // get the crc32 of the data block
        uint32_t crc_32 = crc32(buffer, read, 0);
        
        // make sure the first data block has the correct id and disc number
        if (blockNum == 0) {
            dataBlockNum++;
            if (memcmp(buffer, discInfo->table + 8, 7) == 0) {
                if (fwrite(buffer, writeSize, 1, outputF) != 1) {
                    fprintf(stderr, "ERROR: could not write block\n");
                    break;
                }
            } else {
                fprintf(stderr, "ERROR: disc info does not match\n");
                break;
            }
        }

        // if this is a junk block skip writing it
        else if (isSame(buffer, junk, read)) {
            if (memcmp(&JUNK_BLOCK_MAGIC_WORD, discInfo->table + ((blockNum + 1) * 8), 8) != 0) {
                fprintf(stderr, "ERROR: Saw a junk block at %zu but expected something else\n", blockNum);
                break;
            }
        }

        // if we got this far we should be a data block
        // make sure our table has the correct address and crc
        else if (memcmp(&dataBlockNum, discInfo->table + ((blockNum + 1) * 8), 4) == 0 &&
                 memcmp(&crc_32, discInfo->table + ((blockNum + 1) * 8) + 4, 4) == 0) {
            if (fwrite(buffer, writeSize, 1, outputF) != 1) {
                fprintf(stderr, "ERROR: could not write block\n");
                break;
            }
            dataBlockNum++;
        }

        else {
            uint32_t address =  ((uint64_t) discInfo->table[((blockNum + 1) * 8) + 3] << 0) +
                                ((uint64_t) discInfo->table[((blockNum + 1) * 8) + 2] << 8) +
                                ((uint64_t) discInfo->table[((blockNum + 1) * 8) + 1] << 16) +
                                ((uint64_t) discInfo->table[((blockNum + 1) * 8) + 0] << 24) +

            fprintf(stderr, "ERROR: Saw a data block but address was wrong at %zu ", blockNum);
            fprintf(stderr, "expected %u but %u is in the table\n", dataBlockNum, address);
            break;
        }

        blockNum++;
    }
    fclose(inputF);
    fclose(outputF);
}