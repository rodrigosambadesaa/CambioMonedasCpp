#ifndef ALGORITMO_CAMBIO_H
#define ALGORITMO_CAMBIO_H

#include "bigint.h"

/*
 * Calcula el cambio con el menor numero de monedas en modo ilimitado cuando
 * el monto cabe en la ruta de programacion dinamica. Para montos BigInt muy
 * grandes conserva una busqueda descendente compatible con precision arbitraria.
 * Retorna 1 si encuentra solución, 0 en caso contrario.
 */
int calcular_cambio_optimo(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion);

/*
 * Calcula el cambio con el menor numero de monedas respetando stock cuando
 * el monto cabe en la ruta de programacion dinamica. Para montos BigInt muy
 * grandes conserva una busqueda descendente con poda por stock.
 * Retorna 1 si encuentra solución, 0 en caso contrario.
 */
int calcular_cambio_optimo_stock(const BigInt *monto, const BigIntArray *denominaciones, const BigIntArray *stock, BigIntArray *solucion);

/* Variantes con rangos y limites (compatibles con la implementacion original). */
int calcular_cambio_optimo_con_rango(const BigInt *monto, const BigIntArray *denominaciones, size_t min_monedas, size_t max_monedas, BigIntArray *solucion);
int calcular_cambio_optimo_con_limite(const BigInt *monto, const BigIntArray *denominaciones, size_t limite_monedas, BigIntArray *solucion);

int calcular_cambio_optimo_stock_con_rango(const BigInt *monto, const BigIntArray *denominaciones, const BigIntArray *stock, size_t min_monedas, size_t max_monedas, BigIntArray *solucion);
int calcular_cambio_optimo_stock_con_limite(const BigInt *monto, const BigIntArray *denominaciones, const BigIntArray *stock, size_t limite_monedas, BigIntArray *solucion);

/* Calculo cercano (maximiza monto alcanzado <= solicitado) con y sin stock. */
int calcular_cambio_cercano_con_rango(const BigInt *monto, const BigIntArray *denominaciones, size_t min_monedas, size_t max_monedas, BigInt *monto_cubierto, BigIntArray *solucion);
int calcular_cambio_cercano_stock_con_rango(const BigInt *monto, const BigIntArray *denominaciones, const BigIntArray *stock, size_t min_monedas, size_t max_monedas, BigInt *monto_cubierto, BigIntArray *solucion);

#endif
