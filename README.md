# ProgVoraz

Proyecto de cambio de monedas escrito en C, originalmente creado en 2014 con NetBeans y actualizado para un flujo moderno orientado a GitHub.

## Que hace

- Calcula cambio en modo de monedas infinitas.
- Calcula cambio en modo con stock limitado.
- Persiste actualizaciones de stock en stock.txt.
- Incluye GUI nativa en Windows y macOS (Swift/AppKit), y GUI portable en terminal para Linux.

## Modernizacion aplicada

- Build simplificado y portable mediante Makefile de una sola capa.
- CI automatizada en GitHub Actions para Linux y Windows.
- Limpieza de artefactos con .gitignore para evitar ruido en commits.
- Documentacion de uso y compilacion actualizada.
- Modularizacion de la gestion de moneda en un modulo dedicado.

## Nueva implementacion de gestion de moneda

La logica de acceso y persistencia de monedas/stock se separo de main.c para mejorar mantenibilidad y pruebas.

- moneda_gestion.h: interfaz publica de carga y persistencia.
- moneda_gestion.c: implementacion de lectura de bloques por moneda y actualizacion de stock.
- main.c: ahora orquesta UI y algoritmo voraz, delegando IO de moneda al nuevo modulo.

Funciones expuestas por el modulo:

- cargar_denominaciones_moneda(nombreMoneda, &resultado)
- cargar_stock_moneda(nombreMoneda, &resultado)
- actualizar_stock_moneda(nombreMoneda, stock)

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

GUI (solo Windows):

```bash
make gui
make run-gui
```

En macOS esos comandos ejecutan GUI nativa Swift/AppKit.
En Linux esos comandos ejecutan GUI portable (panel administrador en terminal).

### Opcion 2: GCC directo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c moneda_gestion.c bigint.c -o progvoraz
./progvoraz
```

En Windows:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c moneda_gestion.c bigint.c -o progvoraz.exe
.\progvoraz.exe
```

### Ejecucion en Linux

En Linux el ejecutable no usa extension `.exe`.

Compilacion:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c moneda_gestion.c bigint.c -o progvoraz
```

Ejecucion:

```bash
./progvoraz
```

## Estructura relevante

- main.c: logica principal y TUI en consola.
- gui_window.c: interfaz grafica de ventana (Windows).
- gui_macos.swift: interfaz grafica nativa para macOS (Swift/AppKit).
- gui_portable.c: interfaz GUI portable para Linux (panel administrador en terminal).
- bigint.c / bigint.h: enteros de precision arbitraria.
- moneda_gestion.c / moneda_gestion.h: carga y persistencia de monedas/stock.
- monedas.txt: denominaciones por moneda.
- stock.txt: stock por denominacion.
- .github/workflows/ci.yml: pipeline de compilacion y prueba rapida.

## Panel Administrador (GUI)

En la GUI (ventana nativa en Windows/macOS, portable en Linux):

1. Selecciona una moneda y pulsa `Cargar`.
2. En `Panel Administrador`, elige denominacion y cantidad.
3. Pulsa `Anadir` o `Quitar` para sumar/restar stock.
4. Los cambios se guardan en `stock.txt` inmediatamente.

## Nota sobre NetBeans

La configuracion legacy de NetBeans ya no es necesaria para compilar ni ejecutar el proyecto y se excluye del versionado con .gitignore.

## Proyecto en Visual Studio Code

El repositorio ya incluye configuracion nativa de VS Code en `.vscode/`:

- `.vscode/tasks.json`: tareas de build y run para consola y GUI.
- `.vscode/launch.json`: perfiles de depuracion para consola y GUI.
- `.vscode/c_cpp_properties.json`: configuracion de IntelliSense para C11.
- `.vscode/extensions.json`: extensiones recomendadas.

Flujo sugerido en VS Code:

1. `Terminal > Run Build Task` para compilar (`build: consola` o `build: gui`).
2. `Run and Debug` para depurar (`Debug Consola (Windows)` o `Debug GUI (Windows)`).
3. `Terminal > Run Task` para ejecutar (`run: consola` o `run: gui`).

## Guia detallada

Consulta EJECUCION.md para ejemplos y flujo paso a paso.

