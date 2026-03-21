FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

# Install project dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        libmicrohttpd-dev \
        libmongoc-dev \
        pkg-config \
        curl
