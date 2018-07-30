https://github.com/LedZeppelin68/gcm-nasos-deux/

block size 0x40000 / 262,144 bytes

GC ISO 0x57058000 / 1,459,978,240 bytes
5,570 blocks

Wii Single Layer ISO 0x118240000 / 4,699,979,776 bytes
17,929 blocks

Wii Dual Layer ISO 0x1FB4E0000 / 8,511,160,320 bytes
32,468 blocks

If we use a single block of size 0x40000 bytes to describe our shrunken image
And the largest image has 32,468 blocks
Then each block will have 8 bytes available to describe it

The first block will be a magic number to identify a shrunken image
00-07 "SHRUNKEN"

Data block - a block of real data
00-03 block number in the shrunken image
04-07 CRC32 of the data block

Generated junk block - a block of junk generated from the fancy algorithm
00-07 FF,FF,'J','U','N','K',FF,FF

No Data
00-07 0x00
Once we see an entry of all 0's we are at the end of our image and can ignore all future blocks, which should also be zero.