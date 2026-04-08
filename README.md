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
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c vectordinamico.c -o progvoraz
```

En Windows:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c vectordinamico.c -o progvoraz.exe
.\progvoraz.exe
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
