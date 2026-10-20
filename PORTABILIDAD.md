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


## Snapshot con bloqueo transaccional

La restauración \`--restore-stock\` utiliza
\`restaurar_snapshot_stock_transaccional\` y el mismo bloqueo que los
consumos, cambios de caja y ajustes de stock. El archivo se valida por
todas las divisas y sus longitudes antes de reemplazarlo mediante
el mecanismo de staging con sustitución de destino usado por el backend.
No se borra primero el inventario de Windows. Se incluyen pruebas de
snapshot incompleto (sin mutación) y restauración correcta.


## Menú interactivo con funciones globales

La consola incluye ahora \`x/convertir\`, \`caja\`, \`historial\`,
\`resumen\`, \`snapshot\`, \`restaurar\`, \`reporte\` y \`json\`,
con alias en inglés. Cada una reutiliza la CLI C++20 **en el mismo
proceso**, sin ejecutar el binario original en C ni comandos shell.
Los modos de caja con inventario y los ajustes de administración emplean
las transacciones nativas de stock. Actions comprueba los menús por
stdin en Linux, Windows y macOS.


## Lanzador de interfaz gráfica

La consola admite \`progvoraz --gui\` igual que la versión C. Abre la GUI
compilada correspondiente a la plataforma sin invocar un shell:
\`CreateProcessA\` en Windows o \`posix_spawn\` en macOS/Linux.
En macOS se admite la interfaz Swift compilada con el nombre
\`progvoraz_gui_swift\` y el ejecutable portable de respaldo.
El ejecutable debe encontrarse junto al programa principal; cuando falta,
la aplicación devuelve un error en lugar de intentar rutas arbitrarias.
La ejecución desde CLI no bloquea a la consola.


## Salida JSON de cambio interactivo

\`--json\` activa en consola el mismo formato de resultado del C original:
\`{"resultado":[{"denominacion":"...", "cantidad":"...", "stock":"..."}]}\`.
El campo \`stock\` solo aparece en modo limitado. Los números se serializan
como cadenas de precisión arbitraria desde BigInt, sin redondeos.
La variable \`PROGVORAZ_JSON=1\` activa el mismo formato.
Se mantiene la interfaz de texto alrededor de los resultados para
preservar la experiencia del programa original.


## Sanitizers de memoria y comportamiento indefinido

\`.github/workflows/native-sanitizers.yml\` recompila de manera independiente
la consola y las dos suites de pruebas bajo AddressSanitizer y
UndefinedBehaviorSanitizer. Comprueba el motor BigInt, el solver,
inventario, procesamiento CSV, JSON interactivo y rutas de error.
Se activa en \`push\`, pull requests y de forma manual. No sustituye
las pruebas de interoperabilidad, GUI y Windows/macOS del CI habitual.

## Soluciones óptimas en importes grandes

Se ha añadido un algoritmo exacto de caminos mínimos por clases residuales para importes BigInt sin stock y denominación máxima de hasta 50.000 céntimos. Minimiza la penalización `máxima_denominación × número_piezas − importe` y completa el resultado con monedas de mayor valor. Solo utiliza esta ruta cuando puede demostrar su optimalidad; añade una regresión para `[4,3,1]`, importe `500006`, en la que el algoritmo voraz no es óptimo.

## Solución diofántica con inventario y dos denominaciones

Los importes BigInt grandes con dos denominaciones (máximas hasta 50.000 céntimos) se resuelven ahora por aritmética diofántica y Euclides extendido, también con stock limitado y restricciones de piezas. No es necesario construir una tabla proporcional al importe; se comprueba la igualdad exacta, los límites y la solución óptima mediante el máximo número factible de piezas de mayor denominación. Las nuevas pruebas abarcan 500.006 céntimos, inventario insuficiente y rangos exactos.

## Límites superiores de piezas y precisión arbitraria

Se ha corregido la interpretación de `SIZE_MAX` en el fallback BigInt: es un centinela que significa **sin límite superior**, no el máximo de piezas admisible. Los importes que requieran más de 2⁶⁴−1 monedas se aceptan cuando no hay límite de piezas, mientras que los máximos explícitos siguen siendo vinculantes. Una prueba genera un importe astronómico con una única denominación de 75.001 céntimos.

## Cambio exacto disperso con stock finito

La ruta C++ para importes superiores al límite de DP incorpora estados BigInt dispersos `(importe, piezas)` si el stock suma como máximo 512 piezas. Los paquetes binarios de unidades evitan enumerar cantidades enormes y el estado conserva las soluciones no mínimas necesarias para rangos exactos. También se soporta cambio cercano con stock limitado. Se incluye un límite de 250.000 estados y pruebas con denominaciones de 800.000, 700.000 y 100.000 céntimos, donde la vía voraz no encuentra una solución existente.

### Reintento de sustitución atómica en Windows

Cuando otro proceso mantiene abierto temporalmente `stock.txt` sin `FILE_SHARE_DELETE`, `MoveFileExA` puede devolver `ERROR_SHARING_VIOLATION`/ `ERROR_ACCESS_DENIED`. Ahora se reintenta exclusivamente el reemplazo atómico, sin soltar `stock.txt.lock`, durante un máximo de dos segundos. No se repite ni recalcula el consumo de monedas; si el reemplazo sigue sin ser posible, se conserva el stock original y se informa del fallo.

### Cierre de lectores antes de sustituir el inventario (Windows)

La transacción C++ cierra ahora explícitamente el `std::ifstream` usado para leer el inventario antes de ejecutar `MoveFileExA`. Dejar abierto ese lector durante la sustitución podía impedirla por permisos `FILE_SHARE_DELETE`, incluso dentro del mismo proceso. El bloqueo separado `stock.txt.lock` permanece adquirido, por lo que cerrar el flujo de lectura no permite otra escritura concurrente cooperante.

### Lecturas de stock durante reemplazos concurrentes

Las cargas de inventario de la CLI C++ reintentan la apertura del archivo durante un máximo de dos segundos si Windows impide transitoriamente el acceso mientras otro proceso está terminando una sustitución atómica. Se vuelve a abrir y leer el archivo completo y **no** se repite una operación económica ya completada. Las escrituras siguen exigiendo el bloqueo `stock.txt.lock`.
