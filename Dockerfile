# Stage 1: Builder — use same Debian version as runtime
FROM debian:bookworm AS builder

WORKDIR /build

# Install build toolchain and dev libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    make \
    libmicrohttpd-dev \
    libbson-dev \
    libmongoc-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source files
COPY *.c *.h ./

# Build with optimizations
RUN gcc -Wall -Wextra -O2 main.c database.c handlers.c -o petstore-api \
    -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 \
    -lmicrohttpd -lbson-1.0 -lmongoc-1.0

# Stage 2: Runtime
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libmicrohttpd12 \
    libmongoc-1.0-0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Run as non-root user
RUN groupadd -r petstore && useradd -r -g petstore petstore

# Copy only the binary
COPY --from=builder /build/petstore-api /app/petstore-api

WORKDIR /app
USER petstore

ENV port=8080
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
    CMD curl -f http://localhost:8080/v2/pet/findByStatus?status=available || exit 1

CMD ["/app/petstore-api"]