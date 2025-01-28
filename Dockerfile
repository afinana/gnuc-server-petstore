# Stage 1: Builder
FROM gcc:latest AS builder

COPY . /app

# Set the working directory
WORKDIR /app

# Copy the source code into the container
COPY . /app

# Install necessary libraries for building
RUN apt-get update && apt-get install -y \
    libmicrohttpd-dev \
    libbson-dev \
    libmongoc-dev \
    && rm -rf /var/lib/apt/lists/*

# Build the application as a static binary
RUN gcc -Wall -Wextra -O3 -static main.c database.c handlers.c -o petstore-server  -I/usr/include/libmongoc-1.
0 -I/usr/include/libbson-1.0 -lmicrohttpd -lbson -lmongoc

FROM gcr.io/distroless/cc
COPY --from=builder /app /app
WORKDIR /app

# Expose the application port
EXPOSE 8080

# Command to run the application
CMD ["/petstore-server"]
