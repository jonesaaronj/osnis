CC = cc
CFLAGS = -std=c99 -Wall
SRC_DIR = src
TARGET = osni

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c src/partition.c src/hash.c src/crc32.c

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
