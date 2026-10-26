# Build-only Dockerfile for the Windows x64 executable.
# The scratch stage exports files; it is not a runtime image.
FROM debian:trixie-slim AS build

RUN apt-get update \
	    && DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y gcc-mingw-w64-x86-64-posix \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY src/ ./src/
COPY gamemode_blacklist.txt ./

RUN mkdir /out \
    && x86_64-w64-mingw32-gcc-posix -Os -Wall -Wextra -Wpedantic -std=c11 \
        -s -municode -mwindows -o /out/gamestate-monitor.exe \
        src/main.c src/detector.c -luser32 \
    && cp gamemode_blacklist.txt /out/

FROM scratch AS artifact
COPY --from=build /out/ /
