#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/app"
cd "$APP_DIR"

currency="$(awk '/^[0-9-]+$/ { next } { print; exit }' monedas.txt)"
if [[ -z "$currency" ]]; then
  echo "No se encontro ninguna moneda valida en monedas.txt"
  exit 1
fi

stock_backup="$(mktemp)"
cp stock.txt "$stock_backup"

report_backup="$(mktemp)"
if [[ -f reporte_global.txt ]]; then
  cp reporte_global.txt "$report_backup"
else
  : > "$report_backup"
fi

cleanup() {
  cp "$stock_backup" stock.txt || true
  cp "$report_backup" reporte_global.txt || true
  rm -f "$stock_backup"
  rm -f "$report_backup" stock_snapshot.txt
}
trap cleanup EXIT

run_case() {
  local name="$1"
  local exe="$2"
  local expected="$3"
  local input="$4"
  local output

  cp "$stock_backup" stock.txt
  output="$($exe <<< "$input")"
  echo "--- $name ---"
  echo "$output"
}

echo "Script de pruebas listo. Ejecute manualmente los casos deseados o adapte el script."
