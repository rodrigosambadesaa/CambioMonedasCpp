#ifdef _WIN32
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "bigint.h"
#include "moneda_gestion.h"

#define MAX_MONEDAS 32
#define MAX_NOMBRE 32

#define ID_COMBO_MONEDA 1001
#define ID_BTN_CARGAR 1002
#define ID_LIST_STOCK 1003
#define ID_COMBO_DENOM 1004
#define ID_EDIT_CANTIDAD 1005
#define ID_BTN_AGREGAR 1006
#define ID_BTN_QUITAR 1007
#define ID_BTN_RECARGAR 1008
#define ID_EDIT_MONTO 1009
#define ID_BTN_CALCULAR 1010
#define ID_LIST_RESULTADO 1011
#define ID_RADIO_LIMITADO 1012
#define ID_RADIO_ILIMITADO 1013

static HWND g_comboMoneda;
static HWND g_listStock;
static HWND g_comboDenom;
static HWND g_editCantidad;
static HWND g_btnAgregar;
static HWND g_btnQuitar;
static HWND g_btnCargar;
static HWND g_btnRecargar;
static HWND g_editMonto;
static HWND g_btnCalcular;
static HWND g_listResultado;
static HWND g_radioLimitado;
static HWND g_radioIlimitado;

static char g_monedas[MAX_MONEDAS][MAX_NOMBRE];
static int g_monedasCount = 0;
static int g_monedaActiva = -1;
static int g_modo_stock_limitado = 1;

static BigIntArray g_denom = {0};
static BigIntArray g_stock = {0};

static int es_numero(const char *s)
{
    size_t i;

    if (s == NULL || s[0] == '\0')
        return 0;

    for (i = 0; s[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    return 1;
}

static void liberar_datos_moneda(void)
{
    bigint_array_free(&g_denom);
    bigint_array_free(&g_stock);
    g_monedaActiva = -1;
}

static int cargar_nombres_moneda(void)
{
    FILE *fp = fopen("monedas.txt", "r");
    char token[256];

    g_monedasCount = 0;
    if (fp == NULL)
        return 0;

    while (fscanf(fp, "%255s", token) == 1)
    {
        if (!es_numero(token))
        {
            int yaExiste = 0;
            for (int i = 0; i < g_monedasCount; i++)
            {
                if (strcmp(g_monedas[i], token) == 0)
                {
                    yaExiste = 1;
                    break;
                }
            }

            if (!yaExiste && g_monedasCount < MAX_MONEDAS)
            {
                size_t n = strlen(token);
                if (n >= MAX_NOMBRE)
                    n = MAX_NOMBRE - 1;
                memcpy(g_monedas[g_monedasCount], token, n);
                g_monedas[g_monedasCount][n] = '\0';
                g_monedasCount++;
            }
        }
    }

    fclose(fp);
    return g_monedasCount > 0;
}

static void refrescar_combo_monedas(void)
{
    SendMessageA(g_comboMoneda, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < g_monedasCount; i++)
        SendMessageA(g_comboMoneda, CB_ADDSTRING, 0, (LPARAM)g_monedas[i]);

    if (g_monedasCount > 0)
        SendMessageA(g_comboMoneda, CB_SETCURSEL, 0, 0);
}

static void mostrar_error(const char *titulo, const char *mensaje)
{
    MessageBoxA(NULL, mensaje, titulo, MB_ICONERROR | MB_OK);
}

static void mostrar_info(const char *titulo, const char *mensaje)
{
    MessageBoxA(NULL, mensaje, titulo, MB_ICONINFORMATION | MB_OK);
}

static void limpiar_resultado_cambio(void)
{
    SendMessageA(g_listResultado, LB_RESETCONTENT, 0, 0);
    SendMessageA(g_listResultado, LB_SETHORIZONTALEXTENT, 0, 0);
}

static void configurar_modo_stock(int limitado)
{
    g_modo_stock_limitado = limitado ? 1 : 0;

    SendMessageA(g_radioLimitado, BM_SETCHECK, g_modo_stock_limitado ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_radioIlimitado, BM_SETCHECK, g_modo_stock_limitado ? BST_UNCHECKED : BST_CHECKED, 0);

    EnableWindow(g_comboDenom, g_modo_stock_limitado);
    EnableWindow(g_editCantidad, g_modo_stock_limitado);
    EnableWindow(g_btnAgregar, g_modo_stock_limitado);
    EnableWindow(g_btnQuitar, g_modo_stock_limitado);
}

static void ajustar_scroll_horizontal_lista(HWND lista, int max_chars)
{
    int extent = max_chars * 8;
    if (extent < 0)
        extent = 0;
    SendMessageA(lista, LB_SETHORIZONTALEXTENT, (WPARAM)extent, 0);
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

static void refrescar_lista_stock(void)
{
    char linea[512];
    int max_chars = 0;

    SendMessageA(g_listStock, LB_RESETCONTENT, 0, 0);
    SendMessageA(g_comboDenom, CB_RESETCONTENT, 0, 0);

    if (g_denom.items == NULL)
        return;

    for (size_t i = 0; i < g_denom.len; i++)
    {
        if (g_modo_stock_limitado && g_stock.items != NULL && i < g_stock.len)
            snprintf(linea, sizeof(linea), "Denom %s c -> stock %s", g_denom.items[i].digits, g_stock.items[i].digits);
        else
            snprintf(linea, sizeof(linea), "Denom %s c -> stock ilimitado", g_denom.items[i].digits);

        if ((int)strlen(linea) > max_chars)
            max_chars = (int)strlen(linea);

        SendMessageA(g_listStock, LB_ADDSTRING, 0, (LPARAM)linea);
        SendMessageA(g_comboDenom, CB_ADDSTRING, 0, (LPARAM)g_denom.items[i].digits);
    }

    ajustar_scroll_horizontal_lista(g_listStock, max_chars + 4);

    if (g_denom.len > 0)
        SendMessageA(g_comboDenom, CB_SETCURSEL, 0, 0);

    limpiar_resultado_cambio();
}

static int calcular_cambio_stock(const BigInt *monto, const BigIntArray *denominaciones, const BigIntArray *stock, BigIntArray *solucion)
{
    BigInt restante = {0};

    if (monto == NULL || denominaciones == NULL || stock == NULL || solucion == NULL)
        return 0;
    if (denominaciones->len != stock->len)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        bigint_array_free(solucion);
        return 0;
    }

    for (size_t i = 0; i < denominaciones->len; i++)
    {
        BigInt max_usables = {0};
        BigInt r = {0};
        BigInt tomar = {0};

        if (bigint_is_zero(&denominaciones->items[i]))
            continue;

        if (!bigint_divmod(&restante, &denominaciones->items[i], &max_usables, &r))
            goto fallo;

        if (bigint_compare(&max_usables, &stock->items[i]) > 0)
        {
            if (!bigint_set(&tomar, &stock->items[i]) && !bigint_init(&tomar, stock->items[i].digits))
                goto fallo;
        }
        else
        {
            if (!bigint_set(&tomar, &max_usables) && !bigint_init(&tomar, max_usables.digits))
                goto fallo;
        }

        if (!bigint_array_set(solucion, i, &tomar))
            goto fallo;

        if (!bigint_is_zero(&tomar))
        {
            BigInt uso = {0};
            BigInt nuevo_restante = {0};

            if (!bigint_multiply(&denominaciones->items[i], &tomar, &uso))
            {
                bigint_free(&uso);
                goto fallo;
            }

            if (!bigint_subtract(&restante, &uso, &nuevo_restante))
            {
                bigint_free(&uso);
                bigint_free(&nuevo_restante);
                goto fallo;
            }

            bigint_free(&restante);
            restante = nuevo_restante;
            bigint_free(&uso);
        }

        bigint_free(&max_usables);
        bigint_free(&r);
        bigint_free(&tomar);

        if (bigint_is_zero(&restante))
            break;

        continue;

    fallo:
        bigint_free(&max_usables);
        bigint_free(&r);
        bigint_free(&tomar);
        bigint_free(&restante);
        bigint_array_free(solucion);
        return 0;
    }

    if (!bigint_is_zero(&restante))
    {
        bigint_free(&restante);
        bigint_array_free(solucion);
        return 0;
    }

    bigint_free(&restante);
    return 1;
}

static int calcular_cambio_ilimitado(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion)
{
    BigInt restante = {0};

    if (monto == NULL || denominaciones == NULL || solucion == NULL)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        bigint_array_free(solucion);
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
            bigint_array_free(solucion);
            return 0;
        }

        if (!bigint_array_set(solucion, i, &q))
        {
            bigint_free(&q);
            bigint_free(&r);
            bigint_free(&restante);
            bigint_array_free(solucion);
            return 0;
        }

        bigint_free(&restante);
        restante = r;
        bigint_free(&q);

        if (bigint_is_zero(&restante))
            break;
    }

    if (!bigint_is_zero(&restante))
    {
        bigint_free(&restante);
        bigint_array_free(solucion);
        return 0;
    }

    bigint_free(&restante);
    return 1;
}

static int leer_monto_cambio(BigInt *monto)
{
    char buffer[256];
    int len = GetWindowTextA(g_editMonto, buffer, (int)sizeof(buffer));

    if (len <= 0)
        return 0;

    return bigint_init(monto, buffer);
}

static int calcular_y_mostrar_cambio(void)
{
    BigInt monto = {0};
    BigIntArray solucion = {0};
    BigIntArray stock_nuevo = {0};
    char linea[512];
    int hay_items = 0;
    int max_chars = 0;

    if (g_monedaActiva < 0)
    {
        mostrar_error("Cambio", "Primero carga una moneda.");
        return 0;
    }

    if (!leer_monto_cambio(&monto))
    {
        mostrar_error("Cambio", "Monto invalido. Usa un entero no negativo en centimos.");
        return 0;
    }

    limpiar_resultado_cambio();

    snprintf(linea, sizeof(linea), "Cambio solicitado: %s c", monto.digits);
    max_chars = (int)strlen(linea);
    SendMessageA(g_listResultado, LB_ADDSTRING, 0, (LPARAM)linea);

    if (g_modo_stock_limitado)
    {
        if (!calcular_cambio_stock(&monto, &g_denom, &g_stock, &solucion))
        {
            SendMessageA(g_listResultado, LB_ADDSTRING, 0, (LPARAM) "No existe devolucion exacta con el stock actual.");
            ajustar_scroll_horizontal_lista(g_listResultado, max_chars + 4);
            bigint_free(&monto);
            bigint_array_free(&solucion);
            return 0;
        }
    }
    else
    {
        if (!calcular_cambio_ilimitado(&monto, &g_denom, &solucion))
        {
            SendMessageA(g_listResultado, LB_ADDSTRING, 0, (LPARAM) "No existe devolucion exacta con denominaciones actuales.");
            ajustar_scroll_horizontal_lista(g_listResultado, max_chars + 4);
            bigint_free(&monto);
            bigint_array_free(&solucion);
            return 0;
        }
    }

    for (size_t i = 0; i < g_denom.len; i++)
    {
        if (bigint_is_zero(&solucion.items[i]))
            continue;

        snprintf(linea, sizeof(linea), "%s c -> %s", g_denom.items[i].digits, solucion.items[i].digits);
        if ((int)strlen(linea) > max_chars)
            max_chars = (int)strlen(linea);
        SendMessageA(g_listResultado, LB_ADDSTRING, 0, (LPARAM)linea);
        hay_items = 1;
    }

    if (!hay_items)
    {
        SendMessageA(g_listResultado, LB_ADDSTRING, 0, (LPARAM) "No se requieren monedas para devolver 0.");
        if ((int)strlen("No se requieren monedas para devolver 0.") > max_chars)
            max_chars = (int)strlen("No se requieren monedas para devolver 0.");
    }

    ajustar_scroll_horizontal_lista(g_listResultado, max_chars + 4);

    if (!g_modo_stock_limitado)
    {
        SetWindowTextA(g_editMonto, "");
        bigint_free(&monto);
        bigint_array_free(&solucion);
        return 1;
    }

    if (!copiar_arreglo_bigint(&g_stock, &stock_nuevo))
    {
        bigint_free(&monto);
        bigint_array_free(&solucion);
        mostrar_error("Cambio", "No se pudo preparar actualizacion de stock.");
        return 0;
    }

    for (size_t i = 0; i < stock_nuevo.len; i++)
    {
        BigInt nuevo_stock = {0};

        if (bigint_is_zero(&solucion.items[i]))
            continue;

        if (!bigint_subtract(&stock_nuevo.items[i], &solucion.items[i], &nuevo_stock))
        {
            bigint_array_free(&stock_nuevo);
            bigint_free(&monto);
            bigint_array_free(&solucion);
            mostrar_error("Cambio", "No se pudo aplicar la devolucion al stock.");
            return 0;
        }

        if (!bigint_array_set(&stock_nuevo, i, &nuevo_stock))
        {
            bigint_free(&nuevo_stock);
            bigint_array_free(&stock_nuevo);
            bigint_free(&monto);
            bigint_array_free(&solucion);
            mostrar_error("Cambio", "No se pudo actualizar stock en memoria.");
            return 0;
        }

        bigint_free(&nuevo_stock);
    }

    if (!actualizar_stock_moneda(g_monedas[g_monedaActiva], &stock_nuevo))
    {
        bigint_array_free(&stock_nuevo);
        bigint_free(&monto);
        bigint_array_free(&solucion);
        mostrar_error("Cambio", "No se pudo persistir la devolucion en stock.txt.");
        return 0;
    }

    bigint_array_free(&g_stock);
    g_stock = stock_nuevo;
    SetWindowTextA(g_editMonto, "");

    bigint_free(&monto);
    bigint_array_free(&solucion);
    return 1;
}

static int cargar_moneda_seleccionada(void)
{
    int idx = (int)SendMessageA(g_comboMoneda, CB_GETCURSEL, 0, 0);

    if (idx < 0 || idx >= g_monedasCount)
        return 0;

    liberar_datos_moneda();

    if (!cargar_denominaciones_moneda(g_monedas[idx], &g_denom))
        return 0;
    if (g_modo_stock_limitado)
    {
        if (!cargar_stock_moneda(g_monedas[idx], &g_stock))
        {
            bigint_array_free(&g_denom);
            return 0;
        }

        if (g_denom.len != g_stock.len)
        {
            liberar_datos_moneda();
            return 0;
        }
    }

    g_monedaActiva = idx;
    refrescar_lista_stock();
    return 1;
}

static int leer_cantidad_delta(BigInt *delta)
{
    char buffer[256];
    int len = GetWindowTextA(g_editCantidad, buffer, (int)sizeof(buffer));

    if (len <= 0)
        return 0;

    return bigint_init(delta, buffer);
}

static int aplicar_cambio_stock(int esSuma)
{
    if (!g_modo_stock_limitado)
    {
        mostrar_error("Administrador", "El panel administrador solo aplica en modo stock limitado.");
        return 0;
    }

    int idxDenom;
    BigInt delta = {0};
    BigInt nuevo = {0};

    if (g_monedaActiva < 0)
    {
        mostrar_error("Administrador", "Primero carga una moneda.");
        return 0;
    }

    idxDenom = (int)SendMessageA(g_comboDenom, CB_GETCURSEL, 0, 0);
    if (idxDenom < 0 || (size_t)idxDenom >= g_stock.len)
    {
        mostrar_error("Administrador", "Selecciona una denominacion valida.");
        return 0;
    }

    if (!leer_cantidad_delta(&delta))
    {
        mostrar_error("Administrador", "Cantidad invalida. Usa un entero no negativo.");
        return 0;
    }

    if (esSuma)
    {
        if (!bigint_add(&g_stock.items[idxDenom], &delta, &nuevo))
        {
            bigint_free(&delta);
            mostrar_error("Administrador", "No se pudo sumar la cantidad.");
            return 0;
        }
    }
    else
    {
        if (bigint_compare(&g_stock.items[idxDenom], &delta) < 0)
        {
            bigint_free(&delta);
            mostrar_error("Administrador", "No puedes quitar mas stock del disponible.");
            return 0;
        }

        if (!bigint_subtract(&g_stock.items[idxDenom], &delta, &nuevo))
        {
            bigint_free(&delta);
            mostrar_error("Administrador", "No se pudo restar la cantidad.");
            return 0;
        }
    }

    if (!bigint_array_set(&g_stock, (size_t)idxDenom, &nuevo))
    {
        bigint_free(&delta);
        bigint_free(&nuevo);
        mostrar_error("Administrador", "No se pudo actualizar el stock en memoria.");
        return 0;
    }

    if (!actualizar_stock_moneda(g_monedas[g_monedaActiva], &g_stock))
    {
        bigint_free(&delta);
        bigint_free(&nuevo);
        mostrar_error("Administrador", "No se pudo persistir el stock en archivo.");
        return 0;
    }

    bigint_free(&delta);
    bigint_free(&nuevo);

    SetWindowTextA(g_editCantidad, "");
    refrescar_lista_stock();
    mostrar_info("Administrador", esSuma ? "Stock actualizado (suma)." : "Stock actualizado (resta).");
    return 1;
}

static void crear_controles(HWND hwnd)
{
    CreateWindowA("STATIC", "Panel Principal", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 20, 220, 20, hwnd, NULL, NULL, NULL);

    CreateWindowA("STATIC", "Moneda:", WS_CHILD | WS_VISIBLE,
                  20, 50, 60, 20, hwnd, NULL, NULL, NULL);

    g_comboMoneda = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                  80, 48, 180, 400, hwnd, (HMENU)ID_COMBO_MONEDA, NULL, NULL);

    g_btnCargar = CreateWindowA("BUTTON", "Cargar", WS_CHILD | WS_VISIBLE,
                                270, 47, 90, 24, hwnd, (HMENU)ID_BTN_CARGAR, NULL, NULL);

    g_btnRecargar = CreateWindowA("BUTTON", "Recargar", WS_CHILD | WS_VISIBLE,
                                  370, 47, 90, 24, hwnd, (HMENU)ID_BTN_RECARGAR, NULL, NULL);

    g_radioLimitado = CreateWindowA("BUTTON", "Stock limitado", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                    20, 76, 130, 20, hwnd, (HMENU)ID_RADIO_LIMITADO, NULL, NULL);

    g_radioIlimitado = CreateWindowA("BUTTON", "Stock ilimitado", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                     160, 76, 130, 20, hwnd, (HMENU)ID_RADIO_ILIMITADO, NULL, NULL);

    configurar_modo_stock(1);

    g_listStock = CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | LBS_NOTIFY,
                                20, 102, 440, 140, hwnd, (HMENU)ID_LIST_STOCK, NULL, NULL);

    CreateWindowA("STATIC", "Panel Administrador", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 250, 220, 20, hwnd, NULL, NULL, NULL);

    CreateWindowA("STATIC", "Denominacion:", WS_CHILD | WS_VISIBLE,
                  20, 285, 90, 20, hwnd, NULL, NULL, NULL);

    g_comboDenom = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                 115, 282, 150, 400, hwnd, (HMENU)ID_COMBO_DENOM, NULL, NULL);

    CreateWindowA("STATIC", "Cantidad:", WS_CHILD | WS_VISIBLE,
                  280, 285, 60, 20, hwnd, NULL, NULL, NULL);

    g_editCantidad = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                   345, 282, 115, 24, hwnd, (HMENU)ID_EDIT_CANTIDAD, NULL, NULL);

    g_btnAgregar = CreateWindowA("BUTTON", "Anadir", WS_CHILD | WS_VISIBLE,
                                 190, 320, 120, 30, hwnd, (HMENU)ID_BTN_AGREGAR, NULL, NULL);

    g_btnQuitar = CreateWindowA("BUTTON", "Quitar", WS_CHILD | WS_VISIBLE,
                                320, 320, 120, 30, hwnd, (HMENU)ID_BTN_QUITAR, NULL, NULL);

    CreateWindowA("STATIC", "Panel Devolucion", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 365, 220, 20, hwnd, NULL, NULL, NULL);

    CreateWindowA("STATIC", "Monto (centimos):", WS_CHILD | WS_VISIBLE,
                  20, 395, 100, 20, hwnd, NULL, NULL, NULL);

    g_editMonto = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                125, 392, 190, 24, hwnd, (HMENU)ID_EDIT_MONTO, NULL, NULL);

    g_btnCalcular = CreateWindowA("BUTTON", "Calcular devolucion", WS_CHILD | WS_VISIBLE,
                                  325, 391, 135, 26, hwnd, (HMENU)ID_BTN_CALCULAR, NULL, NULL);

    g_listResultado = CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | LBS_NOTIFY,
                                    20, 425, 440, 110, hwnd, (HMENU)ID_LIST_RESULTADO, NULL, NULL);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        crear_controles(hwnd);
        if (!cargar_nombres_moneda())
        {
            mostrar_error("Inicio", "No se pudieron leer monedas desde monedas.txt");
        }
        refrescar_combo_monedas();
        break;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == ID_BTN_CARGAR)
        {
            if (!cargar_moneda_seleccionada())
                mostrar_error("Carga", "No se pudo cargar moneda/stock.");
            return 0;
        }

        if (id == ID_BTN_RECARGAR)
        {
            int idx_prev = (int)SendMessageA(g_comboMoneda, CB_GETCURSEL, 0, 0);
            if (!cargar_nombres_moneda())
            {
                mostrar_error("Recarga", "No se pudieron recargar monedas.");
                return 0;
            }

            liberar_datos_moneda();
            refrescar_combo_monedas();

            if (idx_prev >= 0 && idx_prev < g_monedasCount)
                SendMessageA(g_comboMoneda, CB_SETCURSEL, (WPARAM)idx_prev, 0);

            if (!cargar_moneda_seleccionada())
            {
                mostrar_error("Recarga", "No se pudo recargar moneda/stock.");
                refrescar_lista_stock();
            }
            return 0;
        }

        if (id == ID_BTN_AGREGAR)
        {
            aplicar_cambio_stock(1);
            return 0;
        }

        if (id == ID_BTN_QUITAR)
        {
            aplicar_cambio_stock(0);
            return 0;
        }

        if (id == ID_BTN_CALCULAR)
        {
            calcular_y_mostrar_cambio();
            return 0;
        }

        if (id == ID_RADIO_LIMITADO || id == ID_RADIO_ILIMITADO)
        {
            int nuevo_limitado = (id == ID_RADIO_LIMITADO) ? 1 : 0;
            if (nuevo_limitado != g_modo_stock_limitado)
            {
                int idx = (int)SendMessageA(g_comboMoneda, CB_GETCURSEL, 0, 0);
                configurar_modo_stock(nuevo_limitado);
                limpiar_resultado_cambio();
                if (idx >= 0 && idx < g_monedasCount)
                    cargar_moneda_seleccionada();
                else
                    refrescar_lista_stock();
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        liberar_datos_moneda();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ProgVorazGUI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc))
        return 1;

    hwnd = CreateWindowA("ProgVorazGUI", "ProgVoraz - Interfaz Grafica",
                         WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT, 500, 590,
                         NULL, NULL, hInstance, NULL);

    if (hwnd == NULL)
        return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
#else
int main(void)
{
    return 0;
}
#endif
