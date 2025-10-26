FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    git pkg-config \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*
# UTF-8 Unterstützung aktivieren
RUN apt-get update && apt-get install -y locales \
    && locale-gen en_US.UTF-8 \
    && update-locale LANG=en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LC_ALL en_US.UTF-8


ARG USER=dev
ARG UID=1000
ARG GID=1000
RUN groupadd -g ${GID} ${USER} && useradd -m -u ${UID} -g ${GID} -s /bin/bash ${USER}

WORKDIR /work
USER ${USER}


