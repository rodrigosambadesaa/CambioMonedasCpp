/*
 * main.c
 *
 * PROPOSITO GENERAL
 * -----------------
 * Este modulo contiene el flujo completo de la aplicacion:
 * 1) Interfaz de consola (TUI simple con menu y cabeceras).
 * 2) Lectura robusta de configuracion de monedas y stock desde archivos.
 * 3) Calculo voraz del cambio en dos modos:
 *    - modo infinito (sin limite de stock)
 *    - modo limitado (descuenta stock disponible)
 * 4) Persistencia del stock actualizado en disco.
 *
 * DISEnO
 * ------
 * - Las funciones son static para limitar su alcance a este archivo.
 * - Se usa validacion defensiva en entradas de usuario y lectura de archivos.
 * - El vector dinamico (vectorP) abstrae el manejo de arreglos de enteros.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vectordinamico.h"

#define MAX_TOKEN 64
#define MAX_MONEDA_NOMBRE 20
#define MAX_DENOMINACIONES 64

/* Limpia la consola y posiciona el cursor arriba a la izquierda (ANSI). */
static void limpiarPantalla(void)
{
    printf("\033[2J\033[H");
}

/* Dibuja una linea horizontal para estructurar visualmente la TUI. */
static void dibujarLinea(void)
{
    printf("+--------------------------------------------------+\n");
}

/* Pinta cabecera principal del programa. */
static void dibujarTitulo(void)
{
    /* Primero se limpia para evitar residuos visuales de ejecuciones previas. */
    limpiarPantalla();
    /* Luego se renderiza marco y titulo. */
    dibujarLinea();
    printf("|          SISTEMA DE CAMBIO DE MONEDAS            |\n");
    dibujarLinea();
}

/*
 * Convierte un texto a entero largo y valida que toda la cadena sea numerica.
 * Devuelve 1 si la conversion es valida, 0 en caso contrario.
 */
static int esNumeroEntero(const char *texto, long *valor)
{
    char *fin;
    long leido;

    /* Rechaza puntero nulo o cadena vacia. */
    if (texto == NULL || *texto == '\0')
        return 0;

    /* strtol deja en 'fin' el primer caracter no convertido. */
    leido = strtol(texto, &fin, 10);
    /* Si no consumio toda la cadena, habia caracteres invalidos. */
    if (*fin != '\0')
        return 0;

    /* Entrega el valor convertido al llamador. */
    *valor = leido;
    return 1;
}

/*
 * Copia un archivo de texto caracter a caracter.
 * Se usa para reemplazar stock.txt desde un temporal de forma segura.
 */
static int copiarArchivo(const char *origen, const char *destino)
{
    FILE *entrada = fopen(origen, "r");
    FILE *salida = fopen(destino, "w");
    int c;

    /* Si cualquiera de los dos archivos falla, se cierra lo que este abierto. */
    if (entrada == NULL || salida == NULL)
    {
        if (entrada != NULL)
            fclose(entrada);
        if (salida != NULL)
            fclose(salida);
        return 0;
    }

    /* Copia secuencial hasta fin de archivo. */
    while ((c = fgetc(entrada)) != EOF)
        fputc(c, salida);

    /* Cierre ordenado de ambos descriptores. */
    fclose(entrada);
    fclose(salida);
    return 1;
}

/*
 * Carga valores asociados a una moneda desde un archivo de bloques.
 * Formato esperado de archivo:
 *   nombre_moneda
 *   valor1
 *   valor2
 *   ...
 *   siguiente_moneda
 *   ...
 *
 * Si convertirMenosUnoACero == 1, el valor -1 se transforma a 0
 * (util para stock, donde -1 significa sin unidades disponibles).
 */
static int cargarDatosMoneda(const char *archivo, const char *nombre, int convertirMenosUnoACero, vectorP *resultado)
{
    FILE *fp = fopen(archivo, "r");
    char token[MAX_TOKEN];
    TELEMENTO valores[MAX_DENOMINACIONES];
    unsigned long cantidad = 0;
    int enSeccion = 0;

    /* Sin archivo no hay datos que cargar. */
    if (fp == NULL)
        return 0;

    /* Lee token a token para tolerar saltos de linea/espacios. */
    while (fscanf(fp, "%63s", token) == 1)
    {
        long numero;
        if (!enSeccion)
        {
            /* Busca el encabezado de la moneda solicitada. */
            if (strcmp(token, nombre) == 0)
                enSeccion = 1;
            continue;
        }

        /* Cuando deja de ser numero, termino la seccion de esa moneda. */
        if (!esNumeroEntero(token, &numero))
            break;

        /* Normaliza el indicador de falta de stock. */
        if (convertirMenosUnoACero && numero == -1)
            numero = 0;

        /* Evita desbordar el arreglo temporal local. */
        if (cantidad >= MAX_DENOMINACIONES)
            break;

        /* Guarda valor y avanza contador. */
        valores[cantidad] = (TELEMENTO)numero;
        cantidad++;
    }

    fclose(fp);

    /* Si no encontro la seccion o no obtuvo datos, falla. */
    if (!enSeccion || cantidad == 0)
        return 0;

    /* Reserva vector dinamico de salida con el tamano exacto. */
    crear(resultado, cantidad);
    if (*resultado == NULL)
        return 0;

    /* Copia de buffer temporal a vector dinamico definitivo. */
    for (unsigned long i = 0; i < cantidad; i++)
        asignar(*resultado, i, valores[i]);

    return 1;
}

/*
 * Reescribe el bloque de una moneda en stock.txt usando stock.tmp.
 * Pasos:
 * 1) Lee stock.txt y escribe stock.tmp.
 * 2) Al detectar la moneda objetivo, reemplaza sus valores por los de 'stock'.
 * 3) Si todo va bien, copia stock.tmp -> stock.txt y borra temporal.
 */
static int actualizarStockArchivo(const char *nombre, vectorP stock)
{
    FILE *entrada = fopen("stock.txt", "r");
    FILE *salida = fopen("stock.tmp", "w");
    char token[MAX_TOKEN];
    int actualizado = 0;
    unsigned long i;

    /* Verifica aperturas y cierra recursos parciales si algo falla. */
    if (entrada == NULL || salida == NULL)
    {
        if (entrada != NULL)
            fclose(entrada);
        if (salida != NULL)
            fclose(salida);
        return 0;
    }

    /* Recorre tokens del archivo original y los replica en temporal. */
    while (fscanf(entrada, "%63s", token) == 1)
    {
        if (!actualizado && strcmp(token, nombre) == 0)
        {
            /* Copia el encabezado de moneda. */
            fprintf(salida, "%s\n", token);

            /* Sustituye cada linea de stock por el estado actual en memoria. */
            for (i = 0; i < tamano(&stock); i++)
            {
                if (fscanf(entrada, "%63s", token) != 1)
                    break;
                /* Mantiene compatibilidad: 0 unidades se guarda como -1. */
                if (recuperar(stock, i) <= 0)
                    fprintf(salida, "-1\n");
                else
                    fprintf(salida, "%d\n", recuperar(stock, i));
            }
            /* Marca que ya actualizo la moneda, para no repetir. */
            actualizado = 1;
        }
        else
        {
            /* Tokens fuera de la seccion objetivo se copian tal cual. */
            fprintf(salida, "%s\n", token);
        }
    }

    fclose(entrada);
    fclose(salida);

    /* Si no encontro la moneda, elimina temporal y reporta fallo. */
    if (!actualizado)
    {
        remove("stock.tmp");
        return 0;
    }

    /* Aplica reemplazo logico del archivo principal. */
    if (!copiarArchivo("stock.tmp", "stock.txt"))
        return 0;

    /* Limpia temporal al finalizar. */
    if (remove("stock.tmp") != 0)
        return 0;

    return 1;
}

/*
 * Algoritmo voraz de cambio sin restriccion de stock.
 * Recorre denominaciones en orden y toma tantas como pueda de cada una.
 */
static int cambio(int x, vectorP valor, vectorP *solucion)
{
    unsigned long i = 0;
    int suma = 0;

    /* Crea vector de cantidades por denominacion (inicia en 0). */
    crear(solucion, tamano(&valor));
    if (*solucion == NULL)
        return 0;

    /* Intenta construir x acumulando monedas vorazmente. */
    while (suma < x && i < tamano(&valor))
    {
        TELEMENTO aux = recuperar(valor, i);
        if (suma + aux <= x)
        {
            /* Toma una moneda de esta denominacion. */
            asignar(*solucion, i, recuperar(*solucion, i) + 1);
            suma += aux;
        }
        else
        {
            /* Pasa a la siguiente denominacion. */
            i++;
        }
    }

    /* Exito solo si se alcanza exactamente x. */
    if (suma == x)
        return 1;

    /* Si falla, deja solucion explicitamente en ceros. */
    for (i = 0; i < tamano(&valor); i++)
        asignar(*solucion, i, 0);
    return 0;
}

/*
 * Variante voraz con stock limitado.
 * Igual que cambio(), pero ademas descuenta cada moneda usada del stock.
 */
static int cambioStock(int x, vectorP valor, vectorP *solucion, vectorP stock)
{
    unsigned long i = 0;
    int suma = 0;

    /* Inicializa solucion a 0 para todas las denominaciones. */
    crear(solucion, tamano(&valor));
    if (*solucion == NULL)
        return 0;

    /* Recorre denominaciones respetando disponibilidad. */
    while (suma < x && i < tamano(&valor))
    {
        TELEMENTO aux = recuperar(valor, i);
        TELEMENTO disponible = recuperar(stock, i);
        if (suma + aux <= x && disponible > 0)
        {
            /* Usa una moneda y descuenta stock. */
            asignar(*solucion, i, recuperar(*solucion, i) + 1);
            asignar(stock, i, disponible - 1);
            suma += aux;
        }
        else
        {
            /* Sin hueco o sin stock: siguiente denominacion. */
            i++;
        }
    }

    /* Exito solo si suma exacta. */
    if (suma == x)
        return 1;

    /* En fallo, deja solucion en ceros (stock ya no se usa para persistir). */
    for (i = 0; i < tamano(&valor); i++)
        asignar(*solucion, i, 0);
    return 0;
}

/*
 * Lee una linea de stdin y elimina salto de linea final.
 * Devuelve 1 si pudo leer, 0 si hubo EOF/error.
 */
static int leerLinea(char *buffer, size_t tam)
{
    if (fgets(buffer, (int)tam, stdin) == NULL)
        return 0;

    /* Reemplaza \r o \n por terminador de cadena. */
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

/*
 * Muestra menu inicial y devuelve opcion normalizada en minuscula.
 * Retorna 0 si la lectura falla o si no se ingreso nada.
 */
static int pedirOpcion(void)
{
    char buffer[16];

    /* Dibuja interfaz de entrada. */
    dibujarTitulo();
    printf("| Elija modo de trabajo:                            |\n");
    printf("|   a) Monedas infinitas                            |\n");
    printf("|   b) Monedas limitadas (usa stock)                |\n");
    dibujarLinea();
    printf("Opcion: ");

    if (!leerLinea(buffer, sizeof(buffer)))
        return 0;

    /* Si esta vacio, se considera invalido. */
    if (buffer[0] == '\0')
        return 0;

    /* tolower evita problemas de 'A'/'B'. */
    return (int)tolower((unsigned char)buffer[0]);
}

/*
 * Solicita cantidad en centimos.
 * Devuelve:
 * - cantidad valida >= 0
 * -1 si entrada invalida
 */
static int pedirCantidad(void)
{
    char buffer[32];
    long valor;

    printf("Cantidad en centimos (0 para salir): ");
    if (!leerLinea(buffer, sizeof(buffer)))
        return -1;

    /* Valida formato numerico y rango razonable para evitar overflow logico. */
    if (!esNumeroEntero(buffer, &valor) || valor < 0 || valor > 1000000)
        return -1;

    return (int)valor;
}

/* Imprime tabla de resultado final, opcionalmente incluyendo stock remanente. */
static void imprimirResultado(vectorP monedas, vectorP solucion, vectorP stock, int usarStock)
{
    unsigned long i;

    dibujarLinea();
    printf("Resultado del cambio\n");
    dibujarLinea();
    for (i = 0; i < tamano(&monedas); i++)
    {
        if (usarStock)
            printf("Moneda %4d c  -> cantidad %3d | stock %3d\n", recuperar(monedas, i), recuperar(solucion, i), recuperar(stock, i));
        else
            printf("Moneda %4d c  -> cantidad %3d\n", recuperar(monedas, i), recuperar(solucion, i));
    }
    dibujarLinea();
}

/*
 * PROGRAMA PRINCIPAL
 * ------------------
 * Orquesta la ejecucion completa:
 * 1) pide modo
 * 2) pide moneda
 * 3) carga datos desde archivos
 * 4) entra en bucle de calculo hasta que usuario ingresa 0
 * 5) libera memoria y termina
 */
int main(void)
{
    char moneda[MAX_MONEDA_NOMBRE + 1];
    int opcion;
    vectorP monedas = NULL;
    vectorP stock = NULL;

    /* 1) Seleccion de modo. */
    opcion = pedirOpcion();
    if (opcion != 'a' && opcion != 'b')
    {
        printf("Opcion no valida.\n");
        return EXIT_FAILURE;
    }

    /* 2) Seleccion de moneda. */
    printf("Nombre de la moneda (ej: euro, dolar, yen): ");
    if (!leerLinea(moneda, sizeof(moneda)) || moneda[0] == '\0')
    {
        printf("Nombre de moneda no valido.\n");
        return EXIT_FAILURE;
    }

    /* 3) Carga de denominaciones desde monedas.txt. */
    if (!cargarDatosMoneda("monedas.txt", moneda, 0, &monedas))
    {
        printf("No se encontro la moneda en monedas.txt.\n");
        return EXIT_FAILURE;
    }

    /* 4) Si modo limitado, carga stock de stock.txt. */
    if (opcion == 'b')
    {
        if (!cargarDatosMoneda("stock.txt", moneda, 1, &stock))
        {
            liberar(&monedas);
            printf("No se encontro stock para la moneda seleccionada.\n");
            return EXIT_FAILURE;
        }
    }

    /* 5) Bucle principal de solicitud de montos y calculo de cambio. */
    while (1)
    {
        int cantidad = pedirCantidad();
        int resultado;
        vectorP solucion = NULL;

        /* Convencion: 0 termina la aplicacion. */
        if (cantidad == 0)
            break;

        /* -1 indica entrada invalida. */
        if (cantidad < 0)
        {
            printf("Entrada invalida. Introduzca un entero positivo.\n");
            continue;
        }

        /* Modo sin stock: solo calcula cantidades. */
        if (opcion == 'a')
        {
            resultado = cambio(cantidad, monedas, &solucion);
            if (resultado)
                imprimirResultado(monedas, solucion, NULL, 0);
            else
                printf("No existe cambio exacto para esa cantidad.\n");
        }
        else
        {
            /* Modo con stock: calcula y descuenta unidades. */
            resultado = cambioStock(cantidad, monedas, &solucion, stock);
            if (resultado)
            {
                imprimirResultado(monedas, solucion, stock, 1);
                /* Persiste el stock actualizado para futuras ejecuciones. */
                if (!actualizarStockArchivo(moneda, stock))
                    printf("No se pudo actualizar el archivo de stock.\n");
            }
            else
            {
                printf("No existe cambio exacto para esa cantidad con el stock actual.\n");
            }
        }

        /* Libera vector temporal de la iteracion actual. */
        liberar(&solucion);
    }

    /* 6) Liberacion final de recursos y salida. */
    liberar(&stock);
    liberar(&monedas);
    printf("Gracias por utilizar este programa.\n");
    return EXIT_SUCCESS;
}
