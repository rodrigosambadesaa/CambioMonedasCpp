#include <stdio.h>
#include <stdlib.h>
#include "vector_dinamico.h"

/*
 * Estructura real del vector dinamico.
 * - datos: memoria contigua de elementos.
 * - tam: cantidad total de posiciones disponibles.
 */
struct STVECTOR
{
    TELEMENTO *datos;
    unsigned long tam;
};

/*
 * Crea un vector de tam1 posiciones.
 * Flujo:
 * 1) valida parametros
 * 2) reserva cabecera
 * 3) reserva arreglo interno
 * 4) inicializa en cero
 */
void crear(vectorP *v1, unsigned long tam1)
{
    unsigned long i;

    /* Si el puntero de salida no existe, no se puede crear nada. */
    if (v1 == NULL)
        return;

    /* Reserva estructura principal. */
    *v1 = (vectorP)malloc(sizeof(struct STVECTOR));
    if (*v1 == NULL)
        return;

    /* Estado inicial consistente antes de reservar datos. */
    (*v1)->datos = NULL;
    (*v1)->tam = tam1;

    /* Vector de tamano cero: estructura valida sin buffer interno. */
    if (tam1 == 0)
        return;

    /* Reserva almacenamiento de elementos. */
    (*v1)->datos = (TELEMENTO *)malloc(tam1 * sizeof(TELEMENTO));
    if ((*v1)->datos == NULL)
    {
        /* Si falla reserva interna, libera cabecera para evitar fuga. */
        free(*v1);
        *v1 = NULL;
        return;
    }

    /* Inicializa todas las celdas a 0 para uso seguro inmediato. */
    for (i = 0; i < tam1; i++)
        (*v1)->datos[i] = 0;
}

/* Asigna un valor en posicion si el vector y el indice son validos. */
void asignar(vectorP v1, unsigned long posicion, TELEMENTO valor)
{
    if (v1 == NULL || posicion >= v1->tam)
        return;

    v1->datos[posicion] = valor;
}

/* Libera memoria del vector y anula el puntero externo. */
void liberar(vectorP *v1)
{
    if (v1 == NULL || *v1 == NULL)
        return;

    /* Se puede liberar NULL sin error, por eso no hace falta condicional extra. */
    free((*v1)->datos);
    free(*v1);
    (*v1) = NULL;
}

/* Obtiene valor en posicion; retorna 0 si indice/puntero es invalido. */
TELEMENTO recuperar(vectorP v1, unsigned long posicion)
{
    if (v1 == NULL || posicion >= v1->tam)
        return 0;

    return v1->datos[posicion];
}

/* Devuelve tamano reservado del vector. */
unsigned long tamano(vectorP *v1)
{
    if (v1 == NULL || *v1 == NULL)
        return 0;

    return (*v1)->tam;
}

