# ---------- Build stage: compile and test ----------
FROM debian:bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libasio-dev libssl-dev libsqlite3-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
COPY legacy ./legacy
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"
# The image is only produced if every API test passes.
RUN ./build/hostelcare_tests tests/fixtures

# ---------- Runtime stage: small image, non-root user ----------
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 libsqlite3-0 libcurl4 ca-certificates tzdata sqlite3 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home /data hostelcare \
    && mkdir -p /data && chown hostelcare /data
COPY --from=build /src/build/hostelcare_api /usr/local/bin/hostelcare_api
COPY demo-data /app/demo-data
ENV TZ=Asia/Kolkata \
    HOSTELCARE_DATA_DIR=/data \
    HOSTELCARE_PORT=18080
USER hostelcare
VOLUME /data
EXPOSE 18080
HEALTHCHECK --interval=30s --timeout=3s CMD ["bash", "-c", "exec 3<>/dev/tcp/127.0.0.1/18080"]
CMD ["hostelcare_api"]
