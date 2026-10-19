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


## Pruebas diferenciales exhaustivas

\`tests/test_bigint_algoritmo.cpp\` contrasta 480 casos
(80 escenarios pseudoaleatorios deterministas × 2 modos × 3 variantes)
contra un oráculo de búsqueda exhaustiva independiente.
Comprueba exactitud del monto, mínimos de piezas, límites de inventario y
cambio cercano. La búsqueda de cambio cercano descarta importe cubierto
cero de acuerdo con el contrato del C original.

## Optimización de cambio cercano

Para límites de monedas con mínimo cero, `solve_near_single_pass` reutiliza una sola tabla dinámica y localiza el mayor importe no nulo alcanzable. Evita el comportamiento cuadrático de calcular todo desde cero para cada céntimo inferior. Las pruebas cubren importes elevados y ausencia de cambio válido.

## GUI con persistencia protegida

La GUI Win32 y la interfaz portable ya no utilizan los valores de stock en memoria para sobrescribir el fichero completo. El panel administrador utiliza `ajustar_stock_moneda_transaccional`; las devoluciones limitadas en Windows utilizan `consumir_stock_moneda_transaccional`. La operación se confirma en pantalla únicamente tras persistirse. En caso de conflicto, el inventario se vuelve a cargar. La GUI nativa Swift de macOS conserva su implementación independiente pendiente de unificación.


## Cambio específico de caja

Se añade el equivalente nativo del modo \`caja / específico\`: validar que
dos combinaciones de denominaciones tienen el mismo valor y, opcionalmente,
actualizar el inventario mediante una sola transacción de recepción y entrega.
Sintaxis: \`--specific MONEDA RECIBIDAS_CSV ENTREGADAS_CSV [--specific-stock]\`.
Las cantidades siguen el orden definido en \`monedas.txt\`. Sin
\`--specific-stock\`, la operación es únicamente una validación. La ruta
con inventario verifica que \`stock + recibido - entregado >= 0\`
para cada denominación y no sobrescribe el fichero en caso de error.
Incluye pruebas de transacción válida, falta de inventario y cantidades
monetarias diferentes en la matriz de GitHub Actions.


## Historial y resumen por divisa

La CLI implementa \`--history [--tail N]\`, con hasta 1000 registros,
y \`--summary EUR\`, con totales de piezas y valor calculados mediante
\`BigInt\`. El fichero \`historial.txt\` utiliza la marca de tiempo
\`[YYYY-MM-DD HH:MM:SS]\` del original. El backend añade un registro
después de cada actualización de stock confirmada. El historial es
*best effort*: no forma una transacción ACID conjunta con el inventario.
