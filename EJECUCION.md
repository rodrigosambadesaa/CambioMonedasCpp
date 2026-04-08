# ProgVoraz - Guia de ejecucion

Este proyecto calcula cambio de monedas en dos modos:

- Modo `a`: monedas infinitas.
- Modo `b`: monedas limitadas por stock (actualiza `stock.txt`).

Tambien incluye:

- Interfaz de consola (TUI) para modo clasico.
- Interfaz grafica de ventana en Windows y macOS con panel de administrador.

## Requisitos

- GCC/Clang disponible en `PATH` (Windows, Linux o macOS).
- Archivos de datos en la carpeta del proyecto:
  - `monedas.txt`
  - `stock.txt`

Comprobacion rapida de GCC:

```bash
gcc --version
```

## Compilar

### Visual Studio Code (recomendado en este repositorio)

El proyecto incluye configuracion en `.vscode/`.

- Build consola: tarea `build: consola`.
- Build GUI: tarea `build: gui`.
- Run consola: tarea `run: consola`.
- Run GUI: tarea `run: gui`.
- Debug: perfiles `Debug Consola (Windows)` y `Debug GUI (Windows)`.

### Opcion recomendada (Makefile moderno)

Desde la raiz del proyecto:

```bash
make debug
```

Compilacion optimizada:

```bash
make release
```

GUI en Windows:

```bash
make gui
```

GUI portable en Linux (sin WinAPI):

```bash
make gui
```

## Modo ventana (GUI) - comandos directos

### Compilar GUI en Windows (GCC directo)

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 gui_window.c moneda_gestion.c bigint.c -o progvoraz_gui.exe -mwindows
```

### Ejecutar GUI en Windows

```powershell
.\progvoraz_gui.exe
```

### Compilar y ejecutar GUI con Make

```bash
make gui
make run-gui
```

### Compilar GUI nativa en macOS (Swift/AppKit)

```bash
swiftc gui_macos.swift -o progvoraz_gui
./progvoraz_gui
```

### GUI portable en Linux (sin interfaz WinAPI)

En Linux, `make gui` compila `gui_portable.c`, que ofrece un panel administrador en terminal con operaciones de anadir/quitar stock.

### Opcion alternativa (GCC directo, Linux)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c moneda_gestion.c bigint.c -o progvoraz
```

### Opcion alternativa (GCC directo, Windows)

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c moneda_gestion.c bigint.c -o progvoraz.exe
```

## Ejecutar

Linux:

```bash
./progvoraz
```

Windows:

```powershell
.\progvoraz.exe
```

GUI Windows:

```powershell
.\progvoraz_gui.exe
```

GUI Linux/macOS:

```bash
./progvoraz_gui
```

Si usaste Make en Linux/macOS:

```bash
./progvoraz
```

## Flujo de uso

1. Elegir opcion:
   - `a` para monedas infinitas.
   - `b` para monedas limitadas.
2. Escribir nombre de moneda (por ejemplo: `euro`, `dolar`, `yen`).
3. Introducir cantidad en centimos.
4. Introducir `0` para salir.

Comandos de navegacion disponibles:

- `volver`: regresa al menu anterior donde aplique.
- `modo`: vuelve al menu de seleccion de modo.
- `salir`: cierra la aplicacion desde cualquier pantalla.

## Panel administrador (GUI)

1. Seleccionar moneda y pulsar `Cargar`.
2. Elegir denominacion.
3. Introducir cantidad (entero no negativo).
4. Pulsar `Anadir` o `Quitar`.

El panel persiste los cambios en `stock.txt` al instante.

## Modos en GUI

La GUI de Windows permite elegir modo directamente:

- `Stock limitado`: calcula devolucion usando stock actual y descuenta del archivo al confirmar.
- `Stock ilimitado`: calcula devolucion ignorando stock (no modifica `stock.txt`).

Para valores de stock muy grandes, las listas de stock y resultado incluyen scroll horizontal para ver el numero completo.

## Ejemplo de prueba automatizada (PowerShell)

Ejecuta una sesion en modo `b`, moneda `dolar`, cantidad `100` y luego sale:

```powershell
@"
b
dolar
100
0
"@ | .\progvoraz.exe
```

## Ejemplo de prueba automatizada (Linux Bash)

Ejecuta una sesion en modo `b`, moneda `dolar`, cantidad `100` y luego sale:

```bash
printf 'b\ndolar\n100\n0\n' | ./progvoraz
```

## Formato esperado de datos

### monedas.txt

Bloques por moneda:

```text
euro
200
100
50
...
dolar
100
50
25
...
```

### stock.txt

Mismo orden de bloques que `monedas.txt`:

- Un numero positivo representa stock disponible.
- `-1` representa sin stock (internamente se trata como `0`).

## Notas tecnicas de la mejora

- Se corrigio la lectura de archivos para evitar errores por uso de `feof`.
- Se corrigieron validaciones de limites en el vector dinamico.
- Se agrego validacion de entrada para opcion y cantidad.
- La actualizacion de stock reescribe `stock.txt` en el mismo archivo abierto (`r+`) sin temporales.
- Se modernizo el flujo para GitHub con `Makefile`, `.gitignore` y CI en `.github/workflows/ci.yml`.

