https://github.com/LedZeppelin68/gcm-nasos-deux/

Open Source Nintendo Image Shrinker

Gamecube/Wii discs seem to work on a block size of 0x40000(262,144) bytes
Here is some useful information about various images

GC ISO 0x57058000(1,459,978,240) bytes
5,570 blocks

Wii Single Layer ISO 0x118240000(4,699,979,776) bytes
17,929 blocks

Wii Dual Layer ISO 0x1FB4E0000(8,511,160,320) bytes
32,468 blocks

If we use a single block of size 0x40000 bytes as a table to describe our shrunken image and if the largest image has 32,468 blocks then each block can have 8 bytes available to describe it within our table

So, here is my proposed format to describe a shrunken gamecube/wii image

The first 8 bit block will just be a magic number to identify a shrunken image
00-07 "SHRUNKEN"

Each block in our full image will be described by an 8 bit section in our table

Data block
00-03 block number where it can be found in the shrunken image
04-07 CRC32 of the data block

Generated junk block - a block of junk generated by the fancy algorithm.
00-07 FF,FF,FF,FF,'J','U','N','K'  Just a magic word to identify junk blocks
The algorithm creates a unique deterministic block of junk given the discId, block number, and disc number.  Since it is completely reproducable and atomic this information can be recreated without knowing any other data from the image.

Repeat junk block - a block of a single uniform repeated byte
00-03 FF,FF,FF,FF 
07    0x?? - the repeated char

No Data
00-07 0x00
Once we see an entry of all 0's we are at the end of our image and can ignore all future blocks, which should also be zero.

This should provide a robust definition of an image that can be used to restore an exact duplicate of the original image as long as the junk generating algorithm is known.  Also, this should be an efficient image for being able to randomly access any given byte of a shrunken image as if it was the original image just by doing a lookup in the table and then either seeking to the location within the shrunken image, or by generating the junk data as necessary.