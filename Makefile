CC = x86_64-w64-mingw32-gcc
CFLAGS = -Os -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = -s -municode -mwindows -luser32
TARGET = dist/gamestate-monitor.exe
SRCS = src/main.c src/detector.c

.PHONY: all build-docker clean
all: $(TARGET)
$(TARGET): $(SRCS) src/detector.h
	mkdir -p dist
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
DOCKER_PLATFORM ?= linux/amd64
build-docker:
	docker buildx build --platform $(DOCKER_PLATFORM) --target artifact --output type=local,dest=dist-docker .
clean:
	$(RM) $(TARGET)
	rm -rf dist-docker
