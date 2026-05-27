FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build --parallel \
    && ctest --test-dir /build --output-on-failure

FROM build AS test

WORKDIR /build

CMD ["ctest", "--output-on-failure"]

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /build/http-proxy-server /usr/local/bin/http-proxy-server
COPY filter.example.json /etc/http-proxy-server/filter.json

EXPOSE 8080

ENTRYPOINT ["http-proxy-server"]

CMD ["--port", "8080", "--filter", "/etc/http-proxy-server/filter.json"]
