
/* Tipo base almacenado por el vector dinamico. */
typedef int TELEMENTO;

/* Puntero opaco a la estructura interna del vector. */
typedef struct STVECTOR *vectorP;

/*
 * Reserva memoria para un vector de tam1 elementos.
 * Inicializa cada posicion a 0.
 */
void crear(vectorP *v1, unsigned long tam1);

/*
 * Asigna un valor en una posicion concreta.
 * Si v1 es nulo o posicion esta fuera de rango, no hace nada.
 */
void asignar(vectorP v1, unsigned long posicion, TELEMENTO valor);

/*
 * Recupera el valor de una posicion.
 * Devuelve 0 si v1 es nulo o la posicion es invalida.
 */
TELEMENTO recuperar(vectorP v1, unsigned long posicion);

/*
 * Libera la memoria del vector y deja el puntero a NULL.
 */
void liberar(vectorP *v1);

/*
 * Devuelve la cantidad de elementos reservados en el vector.
 * Si no existe, devuelve 0.
 */
unsigned long tamano(vectorP *v1);
