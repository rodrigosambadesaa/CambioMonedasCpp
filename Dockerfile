FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential ca-certificates g++ make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Compila el ejecutable principal y la GUI portable
RUN g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic main.cpp moneda_gestion.cpp bigint.cpp vector_dinamico.cpp algoritmo_cambio.cpp -o progvoraz || true
RUN g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic gui_portable.cpp moneda_gestion.cpp bigint.cpp algoritmo_cambio.cpp -o progvoraz_gui || true

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libc6 bash \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/progvoraz /app/progvoraz
COPY --from=builder /app/progvoraz_gui /app/progvoraz_gui
COPY --from=builder /app/monedas.txt /app/monedas.txt
COPY --from=builder /app/stock.txt /app/stock.txt
COPY --from=builder /app/tests /app/tests
COPY --from=builder /app/docker /app/docker

CMD ["/bin/bash"]
