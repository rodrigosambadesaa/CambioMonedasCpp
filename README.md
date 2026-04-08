# ProgVoraz

Proyecto de cambio de monedas escrito en C, originalmente creado en 2014 con NetBeans y actualizado para un flujo moderno orientado a GitHub.

## Que hace

- Calcula cambio en modo de monedas infinitas.
- Calcula cambio en modo con stock limitado.
- Persiste actualizaciones de stock en stock.txt.

## Modernizacion aplicada

- Build simplificado y portable mediante Makefile de una sola capa.
- CI automatizada en GitHub Actions para Linux y Windows.
- Limpieza de artefactos con .gitignore para evitar ruido en commits.
- Documentacion de uso y compilacion actualizada.
- Modularizacion de la gestion de moneda en un modulo dedicado.

## Nueva implementacion de gestion de moneda

La logica de acceso y persistencia de monedas/stock se separo de main.c para mejorar mantenibilidad y pruebas.

- monedagestion.h: interfaz publica de carga y persistencia.
- monedagestion.c: implementacion de lectura de bloques por moneda y actualizacion de stock.
- main.c: ahora orquesta UI y algoritmo voraz, delegando IO de moneda al nuevo modulo.

Funciones expuestas por el modulo:

- cargarDenominacionesMoneda(nombreMoneda, &resultado)
- cargarStockMoneda(nombreMoneda, &resultado)
- actualizarStockMoneda(nombreMoneda, stock)

La persistencia de stock mantiene el enfoque sin archivos temporales:

1. Abre stock.txt en modo lectura/escritura (r+).
2. Carga lineas en memoria.
3. Reemplaza la seccion de la moneda objetivo.
4. Reescribe el mismo archivo y trunca el remanente.

## Requisitos

- GCC en PATH.
- Archivos monedas.txt y stock.txt en la raiz del proyecto.

## Compilar y ejecutar

### Opcion 1: Make

```bash
make debug
make run
```

Para release:

```bash
make release
```

### Opcion 2: GCC directo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c monedagestion.c bigint.c -o progvoraz
./progvoraz
```

En Windows:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c monedagestion.c bigint.c -o progvoraz.exe
.\progvoraz.exe
```

### Ejecucion en Linux

En Linux el ejecutable no usa extension `.exe`.

Compilacion:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c monedagestion.c bigint.c -o progvoraz
```

Ejecucion:

```bash
./progvoraz
```

## Estructura relevante

- main.c: logica principal y TUI en consola.
- vectordinamico.c / vectordinamico.h: abstraccion de vector dinamico.
- monedas.txt: denominaciones por moneda.
- stock.txt: stock por denominacion.
- .github/workflows/ci.yml: pipeline de compilacion y prueba rapida.

## Nota sobre NetBeans

La configuracion legacy de NetBeans ya no es necesaria para compilar ni ejecutar el proyecto y se excluye del versionado con .gitignore.

## Guia detallada

Consulta EJECUCION.md para ejemplos y flujo paso a paso.
