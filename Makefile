CC = cc
CFLAGS = -std=c99 -Wall
SRC_DIR = src
TARGET = osni

all: clean $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/image.c src/disc_info.c src/hash.c src/crc32.c

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
