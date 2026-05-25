# Stage 1: Builder
FROM debian:bookworm AS builder

WORKDIR /build

# Install build toolchain, hiredis, libbson, and other dev libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libc6-dev \
    make \
    pkg-config \
    libmicrohttpd-dev \
    libhiredis-dev \
    libbson-dev \
    libcjson-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source and build files
COPY *.c *.h Makefile ./

# Build using the Makefile
RUN make all \
    "CFLAGS=-Wall -Wextra -g -O2 $(pkg-config --cflags hiredis libbson-1.0)" \
    "LDFLAGS=$(pkg-config --libs hiredis libbson-1.0) -lmicrohttpd -lcjson -lpthread"

# Stage 2: Runtime
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libmicrohttpd12 \
    libhiredis0.14 \
    libbson-1.0-0 \
    libcjson1 \
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
