/*
 * validaciones.h
 * ---------------------------------------------------------------------------
 * Lectura segura de datos por consola. Todas las funciones leen una linea
 * completa con fgets() y la convierten con strtol()/strtod(), por lo que
 * nunca quedan caracteres basura en el buffer ni hay bucles infinitos ante
 * letras, EOF o entradas demasiado largas.
 * ---------------------------------------------------------------------------
 */
#ifndef VALIDACIONES_H
#define VALIDACIONES_H

/* Pide un entero en [min, max]; repite hasta que sea valido. */
int    leer_entero(const char *mensaje, int min, int max);

/* Pide un real en [min, max]; acepta coma o punto decimal. */
double leer_flotante(const char *mensaje, double min, double max);

/* Pide s/n (acepta s, si, n, no en cualquier capitalizacion). */
int    leer_si_no(const char *mensaje);

/* Espera que el usuario presione ENTER. */
void   esperar_enter(void);

#endif /* VALIDACIONES_H */
