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

No se afirma paridad total: servidor HTTP, API de tasas reales, JSON remoto,
historial exhaustivo y nueva GUI multiplataforma requieren ports adicionales.
\`--convert EUR USD\` falla de forma explícita si no existe proveedor de tasas.

Compilar con \`make release\` y probar con \`make test\`.
Para evitar falsear equivalencias, el código C original no se ha añadido como
dependencia de compilación.
