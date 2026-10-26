CC = x86_64-w64-mingw32-gcc
CFLAGS = -Os -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = -s -municode -mwindows -luser32
TARGET = dist/gamestate-monitor.exe
SRCS = src/main.c src/detector.c

.PHONY: all clean
all: $(TARGET)
$(TARGET): $(SRCS) src/detector.h
	mkdir -p dist
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
clean:
	$(RM) $(TARGET)
