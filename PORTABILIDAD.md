# Portabilidad C++20 frente a C

Esta rama incorpora una CLI no interactiva escrita en C++20, sin cargar ni
ejecutar código C del repositorio de referencia.

## Modos

- Sin argumentos: TUI interactiva existente y GUI existente según plataforma.
- \`--input fichero.csv [--output resultado.csv]\`: procesamiento por lotes.
- \`--stream\` / \`--docker\`: CSV por stdin/stdout.
- \`--lang en\`: cabecera y diagnósticos CSV en inglés.
- \`--export-stock-json archivo.json\`: exportación de las nueve monedas.
- \`--export-report reporte.txt\`: valores y piezas BigInt.
- \`--snapshot-stock copia.txt\` y \`--restore-stock copia.txt\`: copia de
  seguridad y restauración después de verificar estructura.
- \`--convert EUR EUR 500\`: conversión identidad y desglose.

El servidor HTTP C++20 expone `GET /health`, `GET /export_stock_json` y
`POST /batch` únicamente por 127.0.0.1. La carga de POST se limita a 1 MiB,
las cabeceras a 32 KiB y no se usan archivos temporales compartidos.

No se afirma paridad total: API de tasas reales, JSON remoto,
historial exhaustivo y nueva GUI multiplataforma requieren ports adicionales.
\`--convert EUR USD\` falla de forma explícita si no existe proveedor de tasas.

Compilar con \`make release\` y probar con \`make test\`.
Para evitar falsear equivalencias, el código C original no se ha añadido como
dependencia de compilación.

## Conversión de divisas determinista

`--convert EUR USD 100` consulta `rates_stub.json` de la versión en C y
redondea céntimos con `BigInt` en C++20, evitando `double` y desbordamientos.
Se admite también `--convert USD EUR 109`. Las tasas del stub son ejemplos
históricos de prueba y **no** cotizaciones actuales. No se ha añadido acceso
HTTP a un proveedor financiero al port C++; si no existe el par en el stub,
la aplicación falla explícitamente.


## Consistencia transaccional de stock

\`moneda_gestion.cpp\` serializa las escrituras cooperantes mediante bloqueo de
\`stock.txt.lock\` (flock/Win32), valida la cantidad de denominaciones antes de
escribir y reemplaza atómicamente \`stock.txt\`. El modo consola aplica consumos
y ajustes desde el último valor persistido; si las piezas ya no están
disponibles, rechaza la operación. Las GUI siguen usando el reemplazo de
inventario absoluto, aunque ahora con escritura protegida, por lo que los
cambios desde snapshots antiguos todavía pueden ser conflictivos. Las
instancias en C# y C++ no comparten mecanismo de bloqueo.
