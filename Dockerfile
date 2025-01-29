# Stage 1: Builder
FROM gcc:latest AS builder

# Set the working directory
WORKDIR /app

# Copy only .c and .h files into the container
COPY *.c *.h /app/

# Install necessary libraries for building
RUN apt-get update && apt-get install -y \
    libmicrohttpd-dev \
    libhiredis-dev \
    libcjson-dev \
    && rm -rf /var/lib/apt/lists/*

# Build the application binary
RUN gcc -Wall -Wextra -O0 main.c database.c handlers.c -o gnuc-server-petstore \
-I/usr/include/hiredis -I/usr/include/cjson \
-lmicrohttpd -lhiredis -lcjson

# Stage 2: Runtime
FROM debian:bookworm-slim

# Install necessary runtime libraries
RUN apt-get update && apt-get install -y \
    libmicrohttpd12 \
    libhiredis0.14 \
    libcjson1 \
    && rm -rf /var/lib/apt/lists/*

# Copy the application binary from the builder stage
COPY --from=builder /app /app

# Set the working directory
WORKDIR /app

# Expose the application port
EXPOSE 8080

# Command to run the application
CMD ["/app/gnuc-server-petstore"]