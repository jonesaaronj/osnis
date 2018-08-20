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

    // Do all of our reading in 0x40000 byte blocks
    unsigned char * buffer = calloc(1, SECTOR_SIZE);

    struct DiscInfo *discInfo = calloc(sizeof(struct DiscInfo), 1);

    // in a shrunken image the first x blocks is always the partition table
    if (fread(buffer, 1, BLOCK_SIZE, inputF) != BLOCK_SIZE){
        fprintf(stderr, "UNSHRINK ERROR: could not read partition table\n");
        return;
    }
    getDiscInfo(discInfo, buffer, 0);

    // the first block of data always exists
    if (fread(buffer, 1, BLOCK_SIZE, inputF) != BLOCK_SIZE){
        fprintf(stderr, "UNSHRINK ERROR: could not read first block\n");
        return;
    }
    if (fwrite(buffer, 1, BLOCK_SIZE, outputF) != BLOCK_SIZE) {
        fprintf(stderr, "UNSHRINK ERROR: could not write first block\n");
        return;
    }
    getDiscInfo(discInfo, buffer, 0);
    printDiscInfo(discInfo);

    size_t discBlockNum = 0;
    size_t lastBlockSize = 0;
    if (discInfo->isWII && discInfo->isDualLayer) {
        discBlockNum = WII_DL_SECTOR_NUM;
    } else if(discInfo->isWII) {
        discBlockNum = WII_SECTOR_NUM;
    } else if(discInfo->isGC) {
        discBlockNum = GC_SECTOR_NUM;
    }

    // fprintf(stderr, "discBlockNum: %zu\n", discBlockNum);
    // fprintf(stderr, "lastBlockSize: %zx\n", lastBlockSize);

    unsigned char * junk = calloc(1, JUNK_BLOCK_SIZE);
    uint32_t lastAddr = 0;
    size_t read = 0;
    for(int blockNum = 2; blockNum <= discBlockNum; blockNum++) {
        
        // set the block size to write
        size_t writeSize = (blockNum < discBlockNum) ? BLOCK_SIZE : lastBlockSize;

        // if FFs we are a junk block
        if (memcmp(&FFs, discInfo->table + (blockNum * 8), 4) == 0) {
            // get the junk block and write it
            getJunkBlock(junk, blockNum - 1, discInfo->discId, discInfo->discNumber);
            // check the crc32 of the junk block and write if everthing is fine
            uint32_t crc = crc32(junk, writeSize, 0);
            if (memcmp(&crc, discInfo->table + (blockNum * 8) + 4, 4) == 0) {
                if (fwrite(junk, writeSize, 1, outputF) != 1) {
                    fprintf(stderr, "UNSHRINK ERROR: could not write block %d\n", blockNum);
                    break;
                }
            } else {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + ((blockNum) * 8) + 4, 4);
                fprintf(stderr, "UNSHRINK ERROR: junk crc error at %d\n", blockNum);
                fprintf(stderr, "UNSHRINK ERROR: Block crc was %x but table crc was %x\n", crc, tableCrc);
                break;
            }
        }

        // if FEs we are a repeat junk block
        else if (memcmp(&FEs, discInfo->table + (blockNum * 8), 4) == 0) {
            unsigned char repeatByte = discInfo->table[((blockNum) * 8) + 7];
            unsigned char repeat[BLOCK_SIZE] = {repeatByte};
            if (fwrite(repeat, writeSize, 1, outputF) != 1) {
                fprintf(stderr, "UNSHRINK ERROR: could not write block %d\n", blockNum);
                break;
            }
        }

        // otherwise we are a data block
        else {
            // only read a new block in if we are not a repeat bock
            if (memcmp(&lastAddr, discInfo->table + (blockNum * 8), 4) != 0) {
                if ((read = fread(buffer, 1, writeSize, inputF)) != writeSize){
                    fprintf(stderr, "UNSHRINK ERROR: could not read block %d\n", blockNum);
                    return;
                }
                if (read != writeSize) {
                    fprintf(stderr, "UNSHRINK ERROR: %d of %zd\n", blockNum, discBlockNum);
                    fprintf(stderr, "UNSHRINK ERROR: read %zx != write %zx\n", read, writeSize);
                    break;
                }
            }
            memcpy(&lastAddr, discInfo->table + (blockNum * 8), 4);
            
            // check the crc32 of the data block and write if everthing is fine
            uint32_t crc = crc32(buffer, writeSize, 0);
            if (memcmp(&crc, discInfo->table + (blockNum * 8) + 4, 4) == 0) {
                if (fwrite(buffer, writeSize, 1, outputF) != 1) {
                    fprintf(stderr, "UNSHRINK ERROR: could not write block %d\n", blockNum);
                    break;
                }
            } else {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + (blockNum * 8) + 4, 4);
                fprintf(stderr, "UNSHRINK ERROR: data crc error at %d\n", blockNum);
                fprintf(stderr, "UNSHRINK ERROR: Block crc was %x but table crc was %x\n", crc, tableCrc);
                break;
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

    // if file pointer is empty read from stdin
    FILE *inputF = (inputFile != NULL) ? fopen(inputFile, "rb") : stdin;
    // if file pointer is empty read from stdout
    FILE *outputF = (outputFile != NULL) ? fopen(outputFile, "wb") : stdout;
    
    // write partition block
    if (fwrite(discInfo->table, (discInfo->tableSectors * SECTOR_SIZE), 1, outputF) != 1) {
        fprintf(stderr, "SHRINK ERROR: could not write partition table\n");
        return;
    }

    // fprintf(stderr, "discBlockNum: %zu\n", discBlockNum);
    // fprintf(stderr, "lastBlockSize: %zx\n", lastBlockSize);

    unsigned char * buffer = calloc(1, SECTOR_SIZE);
    unsigned char * junk = calloc(1, JUNK_BLOCK_SIZE);
    unsigned char * repeatByte;
    uint32_t prevCrc = 0;
    uint32_t dataSector = 0;
    size_t sector = 0;
    size_t read;
    while((read = fread(buffer, 1, SECTOR_SIZE, inputF)) > 0) {

        if (read != SECTOR_SIZE) {
            fprintf(stderr, "SHRINK ERROR: %zd of %zd\n", sector, discInfo->sectors);
        }

        // get the junk block
        // if (sector % JUNK_SECTOR_SIZE == 0) {
        getJunkBlock(junk, sector / JUNK_SECTOR_SIZE, discInfo->discId, discInfo->discNumber);
        // }

        // get the crc32 of the data block
        uint32_t crc = crc32(buffer, read, 0);

        // if this is a junk block skip writing it
        if (memcmp(buffer, junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), read) == 0) {
            if (memcmp(&FFs, discInfo->table + ((sector + 1) * 8), 4) != 0) {
                fprintf(stderr, "SHRINK ERROR: Saw a junk block at %zu but expected something else\n", sector);
                break;
            }
            if (memcmp(&crc, discInfo->table + ((sector + 1) * 8) + 4, 4) != 0) {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + ((sector + 1) * 8) + 4, 4);
                fprintf(stderr, "SHRINK ERROR: junk crc error at %lu\n", sector);
                fprintf(stderr, "SHRINK ERROR: Block crc was %x but table crc was %x\n", crc, tableCrc);
                break;
            }
        }

        // if this is a repeated block just check that the partition table is correct and don't write it
        else if((repeatByte = isUniform(buffer, read)) != NULL) {
            if (memcmp(&FEs, discInfo->table + ((sector + 1) * 8), 4) != 0) {
                fprintf(stderr, "SHRINK ERROR: Saw a repeat block at %zu but expected something else\n", sector);
                break;
            }
            if (memcmp(repeatByte, discInfo->table + ((sector + 1) * 8) + 7, 1) != 0) {
                fprintf(stderr, "SHRINK ERROR: Saw a repeat block at %zu but the repeat byte was wrong\n", sector);
                break;
            }
        }

        // if we got this far we should be a data block
        // make sure our table has the correct address and crc
        else {
            if (prevCrc != crc) {
                dataSector++;
            }
            if (memcmp(&dataSector, discInfo->table + ((sector + 1) * 8), 4) != 0) {
                uint32_t address;
                memcpy(&address, discInfo->table + ((sector + 1) * 8), 4);
                fprintf(stderr, "SHRINK ERROR: Saw a data block but address was wrong at %zu\n", sector);
                fprintf(stderr, "SHRINK ERROR: expected %u but %u is in the table\n", dataSector, address);
                break;
            }
            if (memcmp(&crc, discInfo->table + ((sector + 1) * 8) + 4, 4) != 0) {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + ((sector + 1) * 8) + 4, 4);
                fprintf(stderr, "SHRINK ERROR: data crc error at %lu\n", sector);
                fprintf(stderr, "SHRINK ERROR: Block crc was %x but table crc was %x\n", crc, tableCrc);
                break;
            }
            // only write the block if this was not a repeat block
            if (prevCrc != crc) {
                if (fwrite(buffer, 1, read, outputF) != read) {
                    fprintf(stderr, "SHRINK ERROR: could not write data block %zu at %d\n", sector, dataSector);
                    break;
                }
            }
            prevCrc = crc;
        }
        sector++;
    }
    fclose(inputF);
    fclose(outputF);
}