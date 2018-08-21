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

    unsigned char * buffer = calloc(1, SECTOR_SIZE);

    struct DiscInfo *discInfo = calloc(sizeof(struct DiscInfo), 1);
    size_t sector = 0;
    do {
        // in a shrunken image the first x blocks is always the partition table
        if (fread(buffer, SECTOR_SIZE, 1, inputF) != 1){
            fprintf(stderr, "UNSHRINK ERROR: could not read partition table\n");
            return;
        }
        getDiscInfo(discInfo, buffer, sector++);
    } while (sector <= discInfo->tableSectors);

    printDiscInfo(discInfo);

    unsigned char * junk = calloc(1, JUNK_BLOCK_SIZE);
    uint32_t lastAddr = 0;
    size_t read = 0;
    for(size_t sector = 0; sector < discInfo->sectors; sector++) {

        // if FFs we are a junk block
        if (memcmp(&FFs, discInfo->table + ((sector + 1) * 8), 4) == 0) {
            // get the junk block and write it
            getJunkBlock(junk, sector / JUNK_SECTOR_SIZE, discInfo->discId, discInfo->discNumber);
            // check the crc32 of the junk block and write if everthing is fine
            uint32_t crc = crc32(junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), SECTOR_SIZE, 0);
            if (memcmp(&crc, discInfo->table + ((sector + 1) * 8) + 4, 4) == 0) {
                if (fwrite(junk + ((sector % JUNK_SECTOR_SIZE) * SECTOR_SIZE), SECTOR_SIZE, 1, outputF) != 1) {
                    fprintf(stderr, "UNSHRINK ERROR: could not write sector %ld\n", sector);
                    break;
                }
            } else {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + ((sector + 1) * 8) + 4, 4);
                fprintf(stderr, "UNSHRINK ERROR: junk crc error at %ld\n", sector);
                fprintf(stderr, "UNSHRINK ERROR: Block crc was %x but table crc was %x\n", crc, tableCrc);
                break;
            }
        }

        // if FEs we are a repeat junk block
        else if (memcmp(&FEs, discInfo->table + ((sector + 1) * 8), 4) == 0) {
            unsigned char repeatByte = discInfo->table[((sector + 1) * 8) + 7];
            unsigned char repeat[SECTOR_SIZE] = {repeatByte};
            if (fwrite(repeat, SECTOR_SIZE, 1, outputF) != 1) {
                fprintf(stderr, "UNSHRINK ERROR: could not write sector %ld\n", sector);
                break;
            }
        }

        // otherwise we are a data block
        else {
            // only read a new block in if we are not a repeat bock
            if (memcmp(&lastAddr, discInfo->table + ((sector + 1) * 8), 4) != 0) {
                if ((read = fread(buffer, 1, SECTOR_SIZE, inputF)) != SECTOR_SIZE){
                    fprintf(stderr, "UNSHRINK ERROR: could not read sector %ld\n", sector);
                    return;
                }
            }
            memcpy(&lastAddr, discInfo->table + ((sector + 1) * 8), 4);
            
            // check the crc32 of the data block and write if everthing is fine
            uint32_t crc = crc32(buffer, SECTOR_SIZE, 0);
            if (memcmp(&crc, discInfo->table + ((sector + 1) * 8) + 4, 4) == 0) {
                if (fwrite(buffer, SECTOR_SIZE, 1, outputF) != 1) {
                    fprintf(stderr, "UNSHRINK ERROR: could not write sector %ld\n", sector);
                    break;
                }
            } else {
                uint32_t tableCrc;
                memcpy(&tableCrc, discInfo->table + ((sector + 1) * 8) + 4, 4);
                fprintf(stderr, "UNSHRINK ERROR: data crc error at %ld\n", sector);
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
    uint32_t dataSector = -1;
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