# ProgVoraz - Guia de ejecucion

Este proyecto calcula cambio de monedas en dos modos:

- Modo `a`: monedas infinitas.
- Modo `b`: monedas limitadas por stock (actualiza `stock.txt`).

Tambien incluye una interfaz grafica de consola (TUI) con cabecera, menu y resultados tabulados para facilitar el uso.

## Requisitos

- Windows con GCC disponible en `PATH`.
- Archivos de datos en la carpeta del proyecto:
  - `monedas.txt`
  - `stock.txt`

Comprobacion rapida de GCC en PowerShell:

```powershell
gcc --version
```

## Compilar

### Opcion recomendada (Makefile moderno)

Desde la raiz del proyecto:

```bash
make debug
```

Compilacion optimizada:

```bash
make release
```

### Opcion alternativa (GCC directo)

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c vectordinamico.c -o progvoraz.exe
```

## Ejecutar

```powershell
.\progvoraz.exe
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
- La actualizacion de stock usa archivo temporal y copia segura a `stock.txt`.
- Se modernizo el flujo para GitHub con `Makefile`, `.gitignore` y CI en `.github/workflows/ci.yml`.
