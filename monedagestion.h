#ifndef MONEDAGESTION_H
#define MONEDAGESTION_H

#include "bigint.h"

/*
 * Carga las denominaciones de una moneda desde monedas.txt.
 * Devuelve 1 si la moneda existe y los datos son validos; 0 en caso contrario.
 */
int cargarDenominacionesMoneda(const char *nombreMoneda, BigIntArray *resultado);

/*
 * Carga el stock de una moneda desde stock.txt.
 * El valor -1 en archivo se normaliza a 0 en memoria.
 * Devuelve 1 si la moneda existe y los datos son validos; 0 en caso contrario.
 */
int cargarStockMoneda(const char *nombreMoneda, BigIntArray *resultado);

/*
 * Persiste en stock.txt el vector de stock de la moneda indicada,
 * reescribiendo el mismo archivo (sin temporales).
 * Devuelve 1 si pudo actualizar; 0 en caso contrario.
 */
int actualizarStockMoneda(const char *nombreMoneda, const BigIntArray *stock);

#endif
