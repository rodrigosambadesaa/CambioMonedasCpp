#ifndef VECTOR_DINAMICO_H
#define VECTOR_DINAMICO_H

/* API historica C/C++ compatible: los elementos son enteros. */
typedef int TELEMENTO;
typedef struct STVECTOR *vectorP;

void crear(vectorP *v1, unsigned long tam1);
void asignar(vectorP v1, unsigned long posicion, TELEMENTO valor);
TELEMENTO recuperar(vectorP v1, unsigned long posicion);
void liberar(vectorP *v1);
unsigned long tamano(vectorP *v1);

/* Crecimiento amortizado, nuevos elementos inicializados a cero.
 * En error de reserva o desbordamiento se conserva el vector original.
 */
unsigned long capacidad_vector(vectorP v1);
int reservar(vectorP v1, unsigned long nueva_capacidad);
int redimensionar(vectorP v1, unsigned long nuevo_tam);
int agregar(vectorP v1, TELEMENTO valor);

#endif
