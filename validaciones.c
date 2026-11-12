/*
 * validaciones.c
 * ---------------------------------------------------------------------------
 * Implementacion de la lectura robusta de datos.
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include "validaciones.h"

#define LONG_BUFFER 64

/* Resultado de leer_linea */
#define LINEA_OK      1
#define LINEA_LARGA   0
#define LINEA_EOF    -1

/* Lee una linea completa. Descarta el excedente si es demasiado larga. */
static int leer_linea(char *buf, int n)
{
    size_t len;
    int c;

    if (fgets(buf, n, stdin) == NULL)
        return LINEA_EOF;

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
        return LINEA_OK;
    }
    /* No hubo '\n': la linea era mas larga que el buffer. Se vacia el resto. */
    while ((c = getchar()) != '\n' && c != EOF)
        ;
    return LINEA_LARGA;
}

/* Si se cierra la entrada (Ctrl+D / Ctrl+Z) se sale limpiamente. */
static void obtener_linea_o_salir(char *buf, int n, int *larga)
{
    int r = leer_linea(buf, n);
    if (r == LINEA_EOF) {
        printf("\n  Entrada finalizada. Saliendo del programa.\n");
        exit(EXIT_FAILURE);
    }
    *larga = (r == LINEA_LARGA);
}

/* Elimina espacios al inicio y al final (modifica el buffer). */
static char *recortar(char *s)
{
    char *fin;
    while (isspace((unsigned char)*s)) s++;
    fin = s + strlen(s);
    while (fin > s && isspace((unsigned char)fin[-1])) fin--;
    *fin = '\0';
    return s;
}

int leer_entero(const char *mensaje, int min, int max)
{
    char buf[LONG_BUFFER], *txt, *fin;
    int larga;
    long valor;

    for (;;) {
        printf("  %s [%d - %d]: ", mensaje, min, max);
        fflush(stdout);
        obtener_linea_o_salir(buf, sizeof buf, &larga);
        if (larga) { printf("  [!] Entrada demasiado larga.\n"); continue; }

        txt = recortar(buf);
        if (*txt == '\0') { printf("  [!] No ingreso ningun valor.\n"); continue; }

        errno = 0;
        valor = strtol(txt, &fin, 10);
        if (fin == txt || *fin != '\0') {
            printf("  [!] '%s' no es un numero entero valido.\n", txt);
            continue;
        }
        if (errno == ERANGE || valor < INT_MIN || valor > INT_MAX ||
            valor < min || valor > max) {
            printf("  [!] El valor debe estar entre %d y %d.\n", min, max);
            continue;
        }
        return (int)valor;
    }
}

double leer_flotante(const char *mensaje, double min, double max)
{
    char buf[LONG_BUFFER], *txt, *fin, *p;
    int larga, valido;
    double valor;

    for (;;) {
        printf("  %s [%.1f - %.1f]: ", mensaje, min, max);
        fflush(stdout);
        obtener_linea_o_salir(buf, sizeof buf, &larga);
        if (larga) { printf("  [!] Entrada demasiado larga.\n"); continue; }

        txt = recortar(buf);
        if (*txt == '\0') { printf("  [!] No ingreso ningun valor.\n"); continue; }

        /* Acepta coma decimal (uso habitual en Argentina). */
        for (p = txt; *p; p++)
            if (*p == ',') *p = '.';

        /* Solo se permiten caracteres numericos: evita "nan", "inf", "0x1F". */
        valido = 1;
        for (p = txt; *p; p++)
            if (!strchr("0123456789+-.eE", *p)) { valido = 0; break; }
        if (!valido) {
            printf("  [!] '%s' no es un numero valido.\n", txt);
            continue;
        }

        errno = 0;
        valor = strtod(txt, &fin);
        if (fin == txt || *fin != '\0' || errno == ERANGE || !isfinite(valor)) {
            printf("  [!] '%s' no es un numero valido.\n", txt);
            continue;
        }
        if (valor < min || valor > max) {
            printf("  [!] El valor debe estar entre %.1f y %.1f.\n", min, max);
            continue;
        }
        return valor;
    }
}

int leer_si_no(const char *mensaje)
{
    char buf[LONG_BUFFER], *txt;
    int larga, i;

    for (;;) {
        printf("  %s (s/n): ", mensaje);
        fflush(stdout);
        obtener_linea_o_salir(buf, sizeof buf, &larga);
        if (larga) { printf("  [!] Entrada demasiado larga.\n"); continue; }

        txt = recortar(buf);
        for (i = 0; txt[i]; i++) txt[i] = (char)tolower((unsigned char)txt[i]);

        if (strcmp(txt, "s") == 0 || strcmp(txt, "si") == 0) return 1;
        if (strcmp(txt, "n") == 0 || strcmp(txt, "no") == 0) return 0;
        printf("  [!] Responda 's' o 'n'.\n");
    }
}

void esperar_enter(void)
{
    char buf[LONG_BUFFER];
    printf("\n  Presione ENTER para continuar...");
    fflush(stdout);
    if (leer_linea(buf, sizeof buf) == LINEA_EOF)
        exit(EXIT_FAILURE);
}
