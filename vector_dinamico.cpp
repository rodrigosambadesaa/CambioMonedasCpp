#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vector_dinamico.h"

struct STVECTOR
{
    TELEMENTO *datos;
    unsigned long tam;
    unsigned long capacidad;
};

void crear(vectorP *v1, unsigned long tam1)
{
    vectorP nuevo;
    if (v1 == NULL)
        return;
    *v1 = NULL;
    if ((uintmax_t)tam1 > SIZE_MAX / sizeof(TELEMENTO))
        return;
    nuevo = (vectorP)calloc(1, sizeof(*nuevo));
    if (nuevo == NULL)
        return;
    if (tam1 > 0)
    {
        nuevo->datos = (TELEMENTO *)calloc((size_t)tam1, sizeof(TELEMENTO));
        if (nuevo->datos == NULL)
        {
            free(nuevo);
            return;
        }
    }
    nuevo->tam = tam1;
    nuevo->capacidad = tam1;
    *v1 = nuevo;
}

int reservar(vectorP v1, unsigned long nueva_capacidad)
{
    TELEMENTO *nuevo;
    if (v1 == NULL)
        return 0;
    if (nueva_capacidad <= v1->capacidad)
        return 1;
    if ((uintmax_t)nueva_capacidad > SIZE_MAX / sizeof(TELEMENTO))
        return 0;
    nuevo = (TELEMENTO *)realloc(v1->datos,
                                (size_t)nueva_capacidad * sizeof(TELEMENTO));
    if (nuevo == NULL)
        return 0;
    memset(nuevo + v1->capacidad, 0,
           (size_t)(nueva_capacidad - v1->capacidad) * sizeof(TELEMENTO));
    v1->datos = nuevo;
    v1->capacidad = nueva_capacidad;
    return 1;
}

int redimensionar(vectorP v1, unsigned long nuevo_tam)
{
    unsigned long capacidad;
    if (v1 == NULL)
        return 0;
    if (nuevo_tam > v1->capacidad)
    {
        capacidad = v1->capacidad ? v1->capacidad : 1;
        while (capacidad < nuevo_tam)
        {
            if (capacidad > ULONG_MAX / 2)
            {
                capacidad = nuevo_tam;
                break;
            }
            capacidad *= 2;
        }
        if (!reservar(v1, capacidad))
            return 0;
    }
    if (nuevo_tam > v1->tam)
        memset(v1->datos + v1->tam, 0,
               (size_t)(nuevo_tam - v1->tam) * sizeof(TELEMENTO));
    else if (nuevo_tam < v1->tam)
        memset(v1->datos + nuevo_tam, 0,
               (size_t)(v1->tam - nuevo_tam) * sizeof(TELEMENTO));
    v1->tam = nuevo_tam;
    return 1;
}

int agregar(vectorP v1, TELEMENTO valor)
{
    unsigned long anterior;
    if (v1 == NULL || v1->tam == ULONG_MAX)
        return 0;
    anterior = v1->tam;
    if (!redimensionar(v1, anterior + 1))
        return 0;
    v1->datos[anterior] = valor;
    return 1;
}

void asignar(vectorP v1, unsigned long posicion, TELEMENTO valor)
{
    if (v1 == NULL || posicion >= v1->tam)
        return;
    v1->datos[posicion] = valor;
}

TELEMENTO recuperar(vectorP v1, unsigned long posicion)
{
    if (v1 == NULL || posicion >= v1->tam)
        return 0;
    return v1->datos[posicion];
}

void liberar(vectorP *v1)
{
    if (v1 == NULL || *v1 == NULL)
        return;
    free((*v1)->datos);
    free(*v1);
    *v1 = NULL;
}

unsigned long tamano(vectorP *v1)
{
    return (v1 == NULL || *v1 == NULL) ? 0 : (*v1)->tam;
}
