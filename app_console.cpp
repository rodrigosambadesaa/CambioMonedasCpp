#include "app_console.h"
#include "portable_cli.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
/*
 * main.cpp
 *
 * PROPOSITO GENERAL
 * -----------------
 * Aplicacion de cambio de monedas con soporte para enteros de tamano arbitrario.
 * Toda la aritmetica de montos, denominaciones, stock y cantidades se realiza
 * mediante BigInt (enteros no negativos sin limite practico de digitos).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bigint.h"
#include "moneda_gestion.h"
#include "algoritmo_cambio.h"

#define MAX_MONEDA_NOMBRE 20

static void limpiar_pantalla(void)
{
    printf("\033[2J\033[H");
}

static void dibujar_linea(void)
{
    printf("+--------------------------------------------------+\n");
}

static void dibujar_titulo(void)
{
    limpiar_pantalla();
    dibujar_linea();
    printf("|          SISTEMA DE CAMBIO DE MONEDAS            |\n");
    dibujar_linea();
}

static int leer_linea(char *buffer, size_t tam)
{
    if (fgets(buffer, (int)tam, stdin) == NULL)
        return 0;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static void a_minusculas(char *texto)
{
    size_t i;

    if (texto == NULL)
        return;

    for (i = 0; texto[i] != '\0'; i++)
        texto[i] = (char)tolower((unsigned char)texto[i]);
}

/*
 * Normaliza texto de clave para comparacion con archivos:
 * - pasa a minusculas
 * - elimina acentos comunes (UTF-8 y Latin-1/CP1252)
 */
static void normalizar_clave(const char *origen, char *destino, size_t tamDestino)
{
    size_t i = 0;
    size_t j = 0;

    if (destino == NULL || tamDestino == 0)
        return;

    destino[0] = '\0';
    if (origen == NULL)
        return;

    while (origen[i] != '\0' && j + 1 < tamDestino)
    {
        unsigned char c = (unsigned char)origen[i];

        if (c == 0xC3 && origen[i + 1] != '\0')
        {
            unsigned char s = (unsigned char)origen[i + 1];
            char reemplazo = '\0';

            if (s == 0xA1 || s == 0x81)
                reemplazo = 'a';
            else if (s == 0xA9 || s == 0x89)
                reemplazo = 'e';
            else if (s == 0xAD || s == 0x8D)
                reemplazo = 'i';
            else if (s == 0xB3 || s == 0x93)
                reemplazo = 'o';
            else if (s == 0xBA || s == 0x9A || s == 0xBC || s == 0x9C)
                reemplazo = 'u';
            else if (s == 0xB1 || s == 0x91)
                reemplazo = 'n';

            if (reemplazo != '\0')
            {
                destino[j++] = reemplazo;
                i += 2;
                continue;
            }
        }

        if (c == 0xE1 || c == 0xC1 || c == 0xA0)
            destino[j++] = 'a';
        else if (c == 0xE9 || c == 0xC9 || c == 0x82 || c == 0x90)
            destino[j++] = 'e';
        else if (c == 0xED || c == 0xCD || c == 0xA1 || c == 0xD6)
            destino[j++] = 'i';
        else if (c == 0xF3 || c == 0xD3 || c == 0xA2 || c == 0xE0)
            destino[j++] = 'o';
        else if (c == 0xFA || c == 0xDA || c == 0xFC || c == 0xDC || c == 0xA3 || c == 0x81 || c == 0x9A)
            destino[j++] = 'u';
        else if (c == 0xF1 || c == 0xD1 || c == 0xA4 || c == 0xA5)
            destino[j++] = 'n';
        else
            destino[j++] = (char)tolower(c);

        i++;
    }

    destino[j] = '\0';
}

/*
 * Global non-trivial menu actions invoke the independently reimplemented C++ CLI
 * in-process. No original C executable or shell commands are used here.
 */
static std::string prompt_value(const char *message)
{
    char buffer[4096];
    printf("%s", message);
    fflush(stdout);
    if (!leer_linea(buffer, sizeof(buffer)))
        throw std::runtime_error("Entrada finalizada.");
    return std::string(buffer);
}

static int menu_global_command(const char *command)
{
    std::vector<std::string> args{"progvoraz"};
    std::string mode(command);
    try
    {
        if (mode == "historial" || mode == "history")
            args.push_back("--history");
        else if (mode == "resumen" || mode == "summary")
        {
            args.push_back("--summary");
            args.push_back(prompt_value("Moneda para el resumen: "));
        }
        else if (mode == "snapshot")
        {
            args.push_back("--snapshot-stock");
            args.push_back(prompt_value("Ruta para copiar stock: "));
        }
        else if (mode == "restaurar" || mode == "restore")
        {
            args.push_back("--restore-stock");
            args.push_back(prompt_value("Ruta del snapshot: "));
        }
        else if (mode == "reporte" || mode == "report")
        {
            args.push_back("--export-report");
            args.push_back(prompt_value("Ruta del reporte: "));
        }
        else if (mode == "json")
        {
            args.push_back("--export-stock-json");
            args.push_back(prompt_value("Ruta del archivo JSON: "));
        }
        else if (mode == "x" || mode == "convertir" || mode == "convert")
        {
            args.push_back("--convert");
            args.push_back(prompt_value("Moneda de origen: "));
            args.push_back(prompt_value("Moneda de destino: "));
            args.push_back(prompt_value("Importe en centimos: "));
        }
        else if (mode == "caja" || mode == "specific")
        {
            args.push_back("--specific");
            args.push_back(prompt_value("Moneda de caja: "));
            args.push_back(prompt_value("Cantidades recibidas, separadas por comas: "));
            args.push_back(prompt_value("Cantidades entregadas, separadas por comas: "));
            std::string answer = prompt_value("Actualizar stock (s/n): ");
            if (answer == "s" || answer == "S" || answer == "si" ||
                answer == "yes" || answer == "y")
                args.push_back("--specific-stock");
        }
        else return 0;
        std::vector<char *> argv;
        for (auto &value : args) argv.push_back(value.data());
        int status = run_portable_cli(static_cast<int>(argv.size()), argv.data());
        if (status != 0) printf("Operacion no completada (codigo %d).\n", status);
        return 1;
    }
    catch (const std::exception &error)
    {
        printf("Operacion no completada: %s\n", error.what());
        return 1;
    }
}

/*
 * Muestra menu inicial y devuelve opcion normalizada en minuscula.
 * Retorna:
 * - 'a' o 'b' si la entrada es valida
 * - 0 si la entrada es invalida/vacia
 * - -1 si hubo EOF/error de lectura
 * - -2 si usuario pide salir
 */
static int pedir_opcion(void)
{
    char buffer[16];
    char comando[16];

    dibujar_titulo();
    printf("| Elija modo de trabajo:                            |\n");
    printf("|   a) Monedas infinitas                            |\n");
    printf("|   b) Monedas limitadas (usa stock)                |\n");
    printf("|   c) Administrador de stock                       |\n");
    printf("|   x) Conversion de divisas                        |\n");
    printf("|   caja, historial, resumen, snapshot, restaurar   |\n");
    printf("|   reporte, json (tambien en ingles)              |\n");
    dibujar_linea();
    printf("Opcion (o 'salir'): ");

    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    a_minusculas(comando);
    if (strcmp(comando, "salir") == 0 || strcmp(comando, "exit") == 0 ||
        strcmp(comando, "quit") == 0)
        return -2;
    if (menu_global_command(comando)) return -9;
    if (strcmp(comando, "a") == 0 || strcmp(comando, "b") == 0 ||
        strcmp(comando, "c") == 0)
        return (int)comando[0];
    return 0;
}

/*
 * Solicita monto y lo parsea como BigInt.
 * Retorna:
 * - 1 si lectura valida
 * - 0 si entrada invalida
 * - -1 si EOF/error
 * - 2 si usuario pide volver a seleccion de moneda ("0" o "volver")
 * - 3 si usuario pide salir
 */
static int pedir_cantidad(BigInt *cantidad)
{
    char buffer[2048];
    char comando[2048];
    BigInt tmp = {0};

    printf("Cantidad en centimos (0, volver, limite, cercano o salir): ");
    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    a_minusculas(comando);

    if (strcmp(comando, "salir") == 0)
        return 3;

    if (strcmp(comando, "limite") == 0 || strcmp(comando, "limit") == 0 ||
        strcmp(comando, "l") == 0)
        return 5;
    if (strcmp(comando, "cercano") == 0 || strcmp(comando, "nearest") == 0)
        return 6;

    if (strcmp(comando, "volver") == 0 || strcmp(comando, "0") == 0)
        return 2;

    if (!bigint_init(&tmp, buffer))
        return 0;

    bigint_free(cantidad);
    *cantidad = tmp;
    return 1;
}

/*
 * Solicita una cantidad para operaciones de administrador.
 * Retorna:
 * - 1 si lectura valida
 * - 0 si entrada invalida
 * - -1 si EOF/error
 * - 2 si usuario pide volver
 * - 3 si usuario pide cambiar de modo
 * - 4 si usuario pide salir
 */
static int pedir_cantidad_admin(BigInt *cantidad)
{
    char buffer[2048];
    char comando[2048];
    BigInt tmp = {0};

    printf("Cantidad (volver, modo o salir): ");
    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    a_minusculas(comando);

    if (strcmp(comando, "salir") == 0)
        return 4;
    if (strcmp(comando, "modo") == 0)
        return 3;
    if (strcmp(comando, "volver") == 0)
        return 2;

    if (!bigint_init(&tmp, buffer))
        return 0;

    bigint_free(cantidad);
    *cantidad = tmp;
    return 1;
}

static int pedir_indice_denominacion(size_t maximo, size_t *indice)
{
    char buffer[64];
    char comando[64];
    char *fin;
    long v;

    printf("Indice de denominacion (1-%zu, o volver/modo/salir): ", maximo);
    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    a_minusculas(comando);

    if (strcmp(comando, "salir") == 0)
        return 4;
    if (strcmp(comando, "modo") == 0)
        return 3;
    if (strcmp(comando, "volver") == 0)
        return 2;

    v = strtol(buffer, &fin, 10);
    if (*fin != '\0' || v < 1 || (size_t)v > maximo)
        return 0;

    *indice = (size_t)(v - 1);
    return 1;
}

static int copiar_arreglo_bigint(const BigIntArray *origen, BigIntArray *destino)
{
    if (origen == NULL || destino == NULL)
        return 0;

    if (!bigint_array_create(destino, origen->len))
        return 0;

    for (size_t i = 0; i < origen->len; i++)
    {
        if (!bigint_array_set(destino, i, &origen->items[i]))
        {
            bigint_array_free(destino);
            return 0;
        }
    }

    return 1;
}

static void limpiar_arreglo(BigIntArray *arr)
{
    if (arr != NULL)
        bigint_array_free(arr);
}

[[maybe_unused]] static int cambio_greedy_legacy(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion)
{
    BigInt restante = {0};

    if (monto == NULL || denominaciones == NULL || solucion == NULL)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        limpiar_arreglo(solucion);
        return 0;
    }

    for (size_t i = 0; i < denominaciones->len; i++)
    {
        BigInt q = {0};
        BigInt r = {0};

        if (bigint_is_zero(&denominaciones->items[i]))
            continue;

        if (!bigint_divmod(&restante, &denominaciones->items[i], &q, &r))
        {
            bigint_free(&q);
            bigint_free(&r);
            bigint_free(&restante);
            limpiar_arreglo(solucion);
            return 0;
        }

        if (!bigint_array_set(solucion, i, &q))
        {
            bigint_free(&q);
            bigint_free(&r);
            bigint_free(&restante);
            limpiar_arreglo(solucion);
            return 0;
        }

        bigint_free(&restante);
        restante = r;
        bigint_free(&q);

        if (bigint_is_zero(&restante))
            break;
    }

    {
        int exito = bigint_is_zero(&restante);
        bigint_free(&restante);
        return exito;
    }
}

[[maybe_unused]] static int cambio_stock_greedy_legacy(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion, BigIntArray *stock)
{
    BigInt restante = {0};
    BigIntArray stockTrabajo = {};

    if (monto == NULL || denominaciones == NULL || solucion == NULL || stock == NULL)
        return 0;
    if (denominaciones->len != stock->len)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!copiar_arreglo_bigint(stock, &stockTrabajo))
    {
        limpiar_arreglo(solucion);
        return 0;
    }

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        limpiar_arreglo(solucion);
        limpiar_arreglo(&stockTrabajo);
        return 0;
    }

    for (size_t i = 0; i < denominaciones->len; i++)
    {
        BigInt maxUsables = {0};
        BigInt r = {0};
        BigInt tomar = {0};

        if (bigint_is_zero(&denominaciones->items[i]))
            continue;

        if (!bigint_divmod(&restante, &denominaciones->items[i], &maxUsables, &r))
            goto fallo;

        if (bigint_compare(&maxUsables, &stockTrabajo.items[i]) > 0)
        {
            if (!bigint_set(&tomar, &stockTrabajo.items[i]) && !bigint_init(&tomar, stockTrabajo.items[i].digits))
                goto fallo;
        }
        else
        {
            if (!bigint_set(&tomar, &maxUsables) && !bigint_init(&tomar, maxUsables.digits))
                goto fallo;
        }

        if (!bigint_array_set(solucion, i, &tomar))
            goto fallo;

        if (!bigint_is_zero(&tomar))
        {
            BigInt uso = {0};
            BigInt nuevoRestante = {0};
            BigInt nuevoStock = {0};

            if (!bigint_multiply(&denominaciones->items[i], &tomar, &uso))
            {
                bigint_free(&uso);
                goto fallo;
            }

            if (!bigint_subtract(&restante, &uso, &nuevoRestante))
            {
                bigint_free(&uso);
                bigint_free(&nuevoRestante);
                goto fallo;
            }

            if (!bigint_subtract(&stockTrabajo.items[i], &tomar, &nuevoStock))
            {
                bigint_free(&uso);
                bigint_free(&nuevoRestante);
                bigint_free(&nuevoStock);
                goto fallo;
            }

            bigint_free(&restante);
            restante = nuevoRestante;

            if (!bigint_set(&stockTrabajo.items[i], &nuevoStock))
            {
                bigint_free(&uso);
                bigint_free(&nuevoStock);
                goto fallo;
            }

            bigint_free(&uso);
            bigint_free(&nuevoStock);
        }

        bigint_free(&maxUsables);
        bigint_free(&r);
        bigint_free(&tomar);

        if (bigint_is_zero(&restante))
            break;

        continue;

    fallo:
        bigint_free(&maxUsables);
        bigint_free(&r);
        bigint_free(&tomar);
        bigint_free(&restante);
        limpiar_arreglo(solucion);
        limpiar_arreglo(&stockTrabajo);
        return 0;
    }

    if (!bigint_is_zero(&restante))
    {
        bigint_free(&restante);
        limpiar_arreglo(solucion);
        limpiar_arreglo(&stockTrabajo);
        return 0;
    }

    for (size_t i = 0; i < stock->len; i++)
    {
        if (!bigint_array_set(stock, i, &stockTrabajo.items[i]))
        {
            bigint_free(&restante);
            limpiar_arreglo(solucion);
            limpiar_arreglo(&stockTrabajo);
            return 0;
        }
    }

    bigint_free(&restante);
    limpiar_arreglo(&stockTrabajo);
    return 1;
}

static int cambio(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion)
{
    return calcular_cambio_optimo(monto, denominaciones, solucion);
}

static int cambio_stock(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion, BigIntArray *stock)
{
    if (!calcular_cambio_optimo_stock(monto, denominaciones, stock, solucion))
        return 0;
    for (size_t i = 0; i < stock->len; ++i) {
        BigInt nuevo = {0};
        if (!bigint_subtract(&stock->items[i], &solucion->items[i], &nuevo) ||
            !bigint_array_set(stock, i, &nuevo)) {
            bigint_free(&nuevo);
            limpiar_arreglo(solucion);
            return 0;
        }
        bigint_free(&nuevo);
    }
    return 1;
}

static void imprimir_resultado(const BigIntArray *monedas, const BigIntArray *solucion, const BigIntArray *stock, int usarStock)
{
    if (const char *enabled = std::getenv("PROGVORAZ_JSON"); enabled && *enabled) {
        printf("{\"resultado\": [");
        for (size_t i = 0; i < monedas->len; ++i) {
            if (i) printf(",");
            printf("{\"denominacion\": \"%s\", \"cantidad\": \"%s\"",
                   monedas->items[i].digits, solucion->items[i].digits);
            if (usarStock && stock)
                printf(", \"stock\": \"%s\"", stock->items[i].digits);
            printf("}");
        }
        printf("]}\n");
        return;
    }
    dibujar_linea();
    printf("Resultado del cambio\n");
    dibujar_linea();

    for (size_t i = 0; i < monedas->len; i++)
    {
        if (usarStock)
            printf("Moneda %s c  -> cantidad %s | stock %s\n", monedas->items[i].digits, solucion->items[i].digits, stock->items[i].digits);
        else
            printf("Moneda %s c  -> cantidad %s\n", monedas->items[i].digits, solucion->items[i].digits);
    }

    dibujar_linea();
}

void app_console_imprimir_resultado(const BigIntArray *monedas,
                                       const BigIntArray *solucion,
                                       const BigIntArray *stock,
                                       int usarStock)
{
    imprimir_resultado(monedas, solucion, stock, usarStock);
}

static void imprimir_stock_administrador(const BigIntArray *monedas, const BigIntArray *stock)
{
    dibujar_linea();
    printf("Panel Administrador (Consola)\n");
    dibujar_linea();
    for (size_t i = 0; i < monedas->len; i++)
        printf("[%zu] Denom %s c -> stock %s\n", i + 1, monedas->items[i].digits, stock->items[i].digits);
    dibujar_linea();
}

[[maybe_unused]] static int aplicar_cambio_administrador(BigIntArray *stock, size_t idx, const BigInt *delta, int esSuma)
{
    BigInt nuevo = {0};

    if (esSuma)
    {
        if (!bigint_add(&stock->items[idx], delta, &nuevo))
            return 0;
    }
    else
    {
        if (bigint_compare(&stock->items[idx], delta) < 0)
            return 0;
        if (!bigint_subtract(&stock->items[idx], delta, &nuevo))
            return 0;
    }

    if (!bigint_array_set(stock, idx, &nuevo))
    {
        bigint_free(&nuevo);
        return 0;
    }

    bigint_free(&nuevo);
    return 1;
}

// The C reference accepts an upper bound (N), exact count (=N),
// or an inclusive range (N-M) in the traditional coin-change submenu.
static bool parse_restriccion(const std::string &raw, size_t *minimum, size_t *maximum)
{
    if (!minimum || !maximum) return false;
    std::string text;
    for (unsigned char c : raw) if (!isspace(c)) text.push_back(static_cast<char>(c));
    if (text.empty()) return false;
    auto parse = [](const std::string &value, size_t *target) {
        if (value.empty()) return false;
        for (unsigned char c : value) if (!isdigit(c)) return false;
        try {
            size_t used=0;
            unsigned long long n=std::stoull(value,&used);
            if (used!=value.size() || n>std::numeric_limits<size_t>::max()) return false;
            *target=static_cast<size_t>(n);
            return true;
        } catch (...) { return false; }
    };
    if (text[0]=='=') {
        if (!parse(text.substr(1),minimum)) return false;
        *maximum=*minimum;
        return true;
    }
    size_t separator=text.find('-');
    if (separator!=std::string::npos) {
        if (!parse(text.substr(0,separator),minimum) ||
            !parse(text.substr(separator+1),maximum)) return false;
        return *minimum<=*maximum;
    }
    *minimum=0;
    return parse(text,maximum);
}

static bool aceptar_sugerencia(void)
{
    char reply[32];
    printf("Aceptar sugerencia? (si/no): ");
    if (!leer_linea(reply,sizeof(reply))) return false;
    a_minusculas(reply);
    return strcmp(reply,"si")==0 || strcmp(reply,"s")==0 ||
           strcmp(reply,"yes")==0 || strcmp(reply,"y")==0;
}

static int flujo_cambio_avanzado(const char *clave, const BigIntArray *denominaciones,
                                BigIntArray *stock, int usar_stock, bool con_limite)
{
    size_t minimum=0,maximum=std::numeric_limits<size_t>::max();
    if (con_limite) {
        char range[128];
        printf("Restriccion de monedas (N, =N, N-M): ");
        if (!leer_linea(range,sizeof(range))) return 0;
        if (!parse_restriccion(range,&minimum,&maximum)) {
            printf("Restriccion invalida.\n");
            return 1;
        }
    }
    BigInt amount{nullptr},covered{nullptr};
    BigIntArray solution{};
    int state=pedir_cantidad(&amount);
    if (state==-1 || state==3) {bigint_free(&amount);return 0;}
    if (state!=1 || bigint_is_zero(&amount)) {
        printf("Importe invalido o cancelado.\n");
        bigint_free(&amount);
        return 1;
    }
    int ok=usar_stock
        ? calcular_cambio_optimo_stock_con_rango(&amount,denominaciones,stock,
                                                 minimum,maximum,&solution)
        : calcular_cambio_optimo_con_rango(&amount,denominaciones,
                                           minimum,maximum,&solution);
    bool partial=false;
    if (!ok) {
        ok=usar_stock
            ? calcular_cambio_cercano_stock_con_rango(&amount,denominaciones,stock,
                        minimum,maximum,&covered,&solution)
            : calcular_cambio_cercano_con_rango(&amount,denominaciones,
                        minimum,maximum,&covered,&solution);
        partial=ok && bigint_compare(&covered,&amount)<0;
    }
    if (!ok) {
        printf("No existe cambio exacto ni sugerencia cercana positiva.\n");
        bigint_free(&amount);bigint_free(&covered);
        bigint_array_free(&solution);
        return 1;
    }
    if (partial) {
        BigInt missing{nullptr};
        if (bigint_subtract(&amount,&covered,&missing))
            printf("Sugerencia cercana: cubrir %s c (faltan %s c).\n",
                   covered.digits,missing.digits);
        bigint_free(&missing);
        if (!aceptar_sugerencia()) {
            printf("Sugerencia rechazada; el stock no se modifica.\n");
            bigint_free(&amount);bigint_free(&covered);
            bigint_array_free(&solution);
            return 1;
        }
    }
    if (usar_stock) {
        BigIntArray updated{};
        if (!consumir_stock_moneda_transaccional(clave,&solution,&updated)) {
            printf("El inventario ha cambiado o no se pudo guardar.\n");
            bigint_array_free(stock);
            cargar_stock_moneda(clave,stock);
            bigint_free(&amount);bigint_free(&covered);
            bigint_array_free(&solution);
            return 1;
        }
        bigint_array_free(stock);
        *stock=updated;
    }
    BigInt total{nullptr};
    bigint_init(&total,"0");
    for (size_t i=0;i<solution.len;++i) {
        BigInt next{nullptr};
        if (!bigint_add(&total,&solution.items[i],&next)) {
            bigint_free(&next);break;
        }
        bigint_free(&total);total=next;
    }
    printf("Cambio aceptado: %s centimos; piezas=%s.\n",
           partial ? covered.digits : amount.digits,
           total.digits ? total.digits : "desconocido");
    app_console_imprimir_resultado(denominaciones,&solution,stock,usar_stock);
    bigint_free(&total);
    bigint_free(&amount);bigint_free(&covered);
    bigint_array_free(&solution);
    return 1;
}

int app_console_run(void)
{
    char moneda[MAX_MONEDA_NOMBRE + 1];
    char monedaClave[MAX_MONEDA_NOMBRE + 1];
    char monedaCmd[MAX_MONEDA_NOMBRE + 1];
    int opcion = 0;
    int ejecutando = 1;
    BigIntArray monedas = {};
    BigIntArray stock = {};

    while (ejecutando)
    {
        if (opcion != 'a' && opcion != 'b' && opcion != 'c')
        {
            while (1)
            {
                opcion = pedir_opcion();
                if (opcion == 'a' || opcion == 'b' || opcion == 'c')
                    break;

                if (opcion == -9)
                    continue;

                if (opcion == -2)
                {
                    ejecutando = 0;
                    break;
                }

                if (opcion == -1)
                {
                    printf("Entrada finalizada.\n");
                    ejecutando = 0;
                    break;
                }

                printf("Opcion no valida. Intente de nuevo.\n");
            }
        }

        if (!ejecutando)
            break;

        while (1)
        {
            limpiar_arreglo(&stock);
            limpiar_arreglo(&monedas);

            printf("Nombre de la moneda (ej: euro, dolar, yen), 'modo', 'volver' o 'salir': ");
            if (!leer_linea(moneda, sizeof(moneda)))
            {
                printf("Entrada finalizada.\n");
                ejecutando = 0;
                break;
            }

            if (moneda[0] == '\0')
            {
                printf("Nombre de moneda no valido. Intente de nuevo.\n");
                continue;
            }

            normalizar_clave(moneda, monedaCmd, sizeof(monedaCmd));
            if (strcmp(monedaCmd, "salir") == 0)
            {
                ejecutando = 0;
                break;
            }

            if (strcmp(monedaCmd, "modo") == 0 || strcmp(monedaCmd, "volver") == 0)
            {
                opcion = 0;
                break;
            }

            normalizar_clave(moneda, monedaClave, sizeof(monedaClave));

            if (!cargar_denominaciones_moneda(monedaClave, &monedas))
            {
                printf("No se encontro la moneda en monedas.txt. Intente de nuevo.\n");
                continue;
            }

            if (opcion == 'b' || opcion == 'c')
            {
                if (!cargar_stock_moneda(monedaClave, &stock))
                {
                    limpiar_arreglo(&monedas);
                    printf("No se encontro stock para la moneda seleccionada. Intente de nuevo.\n");
                    continue;
                }

                if (stock.len != monedas.len)
                {
                    limpiar_arreglo(&stock);
                    limpiar_arreglo(&monedas);
                    printf("Inconsistencia entre denominaciones y stock. Intente otra moneda.\n");
                    continue;
                }
            }

            while (1)
            {
                if (opcion == 'c')
                {
                    char accion[32];
                    int esSuma;
                    size_t idxDenom;
                    int estadoIndice;
                    BigInt delta = {0};
                    int estadoDelta;

                    imprimir_stock_administrador(&monedas, &stock);
                    printf("Accion admin (anadir/quitar, volver, modo, salir): ");
                    if (!leer_linea(accion, sizeof(accion)))
                    {
                        printf("Entrada finalizada.\n");
                        ejecutando = 0;
                        break;
                    }

                    a_minusculas(accion);
                    if (strcmp(accion, "salir") == 0)
                    {
                        ejecutando = 0;
                        break;
                    }
                    if (strcmp(accion, "modo") == 0)
                    {
                        opcion = 0;
                        break;
                    }
                    if (strcmp(accion, "volver") == 0)
                        break;

                    if (strcmp(accion, "anadir") == 0)
                        esSuma = 1;
                    else if (strcmp(accion, "quitar") == 0)
                        esSuma = 0;
                    else
                    {
                        printf("Accion invalida.\n");
                        continue;
                    }

                    estadoIndice = pedir_indice_denominacion(monedas.len, &idxDenom);
                    if (estadoIndice == -1)
                    {
                        printf("Entrada finalizada.\n");
                        ejecutando = 0;
                        break;
                    }
                    if (estadoIndice == 4)
                    {
                        ejecutando = 0;
                        break;
                    }
                    if (estadoIndice == 3)
                    {
                        opcion = 0;
                        break;
                    }
                    if (estadoIndice == 2)
                        continue;
                    if (estadoIndice == 0)
                    {
                        printf("Indice invalido.\n");
                        continue;
                    }

                    estadoDelta = pedir_cantidad_admin(&delta);
                    if (estadoDelta == -1)
                    {
                        printf("Entrada finalizada.\n");
                        bigint_free(&delta);
                        ejecutando = 0;
                        break;
                    }
                    if (estadoDelta == 4)
                    {
                        bigint_free(&delta);
                        ejecutando = 0;
                        break;
                    }
                    if (estadoDelta == 3)
                    {
                        bigint_free(&delta);
                        opcion = 0;
                        break;
                    }
                    if (estadoDelta == 2)
                    {
                        bigint_free(&delta);
                        continue;
                    }
                    if (estadoDelta == 0)
                    {
                        bigint_free(&delta);
                        printf("Cantidad invalida.\n");
                        continue;
                    }

                    BigIntArray stockNuevo = {};
                    if (!ajustar_stock_moneda_transaccional(monedaClave, idxDenom,
                                                            &delta, esSuma, &stockNuevo))
                    {
                        printf("No se pudo aplicar el cambio de stock (revisa disponibilidad).\n");
                        bigint_array_free(&stock);
                        cargar_stock_moneda(monedaClave, &stock);
                    }
                    else
                    {
                        bigint_array_free(&stock);
                        stock = stockNuevo;
                        printf("Stock actualizado correctamente.\n");
                    }

                    bigint_free(&delta);
                    continue;
                }

                BigInt cantidad = {0};
                int estadoCantidad;
                int resultado;
                BigIntArray solucion = {};

                estadoCantidad = pedir_cantidad(&cantidad);
                if (estadoCantidad == -1)
                {
                    printf("Entrada finalizada.\n");
                    bigint_free(&cantidad);
                    ejecutando = 0;
                    break;
                }

                if (estadoCantidad == 5 || estadoCantidad == 6)
                {
                    int status = flujo_cambio_avanzado(monedaClave,&monedas,&stock,
                                                     opcion=='b',estadoCantidad==5);
                    bigint_free(&cantidad);
                    if (!status) {ejecutando=0;break;}
                    continue;
                }

                if (estadoCantidad == 2)
                {
                    bigint_free(&cantidad);
                    break;
                }

                if (estadoCantidad == 3)
                {
                    bigint_free(&cantidad);
                    ejecutando = 0;
                    break;
                }

                if (estadoCantidad == 0)
                {
                    printf("Entrada invalida. Introduzca un entero no negativo.\n");
                    bigint_free(&cantidad);
                    continue;
                }

                if (opcion == 'a')
                {
                    resultado = cambio(&cantidad, &monedas, &solucion);
                    if (resultado)
                        imprimir_resultado(&monedas, &solucion, NULL, 0);
                    else
                        printf("No existe cambio exacto para esa cantidad.\n");
                }
                else
                {
                    resultado = cambio_stock(&cantidad, &monedas, &solucion, &stock);
                    if (resultado)
                    {
                        BigIntArray stockNuevo = {};
                        if (consumir_stock_moneda_transaccional(monedaClave, &solucion, &stockNuevo))
                        {
                            bigint_array_free(&stock);
                            stock = stockNuevo;
                            imprimir_resultado(&monedas, &solucion, &stock, 1);
                        }
                        else
                        {
                            printf("Inventario modificado por otro proceso o error al persistir.\n");
                            bigint_array_free(&stock);
                            cargar_stock_moneda(monedaClave, &stock);
                        }
                    }
                    else
                    {
                        printf("No existe cambio exacto para esa cantidad con el stock actual.\n");
                    }
                }

                limpiar_arreglo(&solucion);
                bigint_free(&cantidad);
            }

            if (!ejecutando)
                break;

            /* Vuelve a seleccionar moneda dentro del mismo modo. */
            break;
        }

        if (!ejecutando)
            break;

        /* Si opcion se reinicio a 0, el usuario pidio cambiar de modo. */
        if (opcion == 0)
            continue;
    }

    limpiar_arreglo(&stock);
    limpiar_arreglo(&monedas);
    printf("Gracias por utilizar este programa.\n");
    return EXIT_SUCCESS;
}
