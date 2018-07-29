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

The second block will contain the Disc id, Disc Number, and Disc Type.  
The second full block from the iso will always be available as the second block in the shrunken image
00-05 Disc Id
06 Disc Number
07 Disc Type 1=GC Single Layer 2=WII Single Layer 3=WII Dual Layer

Then each subsequent block is described as such

Data block - a block of real data
00-07 the block number where this block can be found in the shrunken image. Just treat the whole 8 bytes like an unsigned int.
Counting always includes the partition block and the first block.
So, if I want a block of data at a particular location just look up that block number in the partition table block
and then pull the full 0x40000 from the shrunken image at the given location

Repeated junk block - a block of repeated bytes
00-06 FF,FF,FF,FF,00,00,00
07 the character/byte being repeated
This is well beyond the max block size of any image so it will never be confused with an actual block address
To recreate this block just repeat the char/byte at 07 0x40000 times and return the block

Generated junk block - a block of junk generated from the fancy algorithm
00-07 FF,FF,FF,FF,FF,FF,FF,FF 
This is well beyond the max block size of any image so it will never be confused with an actual block address
The block number will still equate directly to the same block number in a real disk so regenerating 
this data will be as simple as calling the generateJunkData function on the discId, discNum and block number


00-07 0x00
Once we see an entry of all 0's we are at the end of our image and can ignore all future blocks, which should also be zero.