CC = cc
MINGW = x86_64-w64-mingw32-gcc
CFLAGS = -std=c99 -Wall
SRC_DIR = src
TARGET = osnis

all: clean $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/image.c src/disc_info.c src/hash.c src/crc32.c src/aes.c

win: src/main.c
	$(MINGW) $(CFLAGS) -o dist/$(TARGET) src/main.c src/image.c src/disc_info.c src/hash.c src/crc32.c src/aes.c

clean:
	rm -f $(TARGET)
	rm -f dist/$(TARGET).*

run: $(TARGET)
	./$(TARGET)
