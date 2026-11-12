/*
 * main.c
 * ---------------------------------------------------------------------------
 * TPI - Algoritmos y Estructuras de Datos (UTN)
 * Mision Artemis - Escenario 1: Ascenso y Propulsion con el SLS
 *
 * Menu interactivo, configuracion de parametros y reporte de desempeno.
 * (Los textos no llevan tildes para verse bien en cualquier consola.)
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include "artemis.h"
#include "validaciones.h"

#define ANCHO 78

/* ------------------------------ Utilidades UI ---------------------------- */

static void linea(char c)
{
    int i;
    for (i = 0; i < ANCHO; i++) putchar(c);
    putchar('\n');
}

static void titulo(const char *texto)
{
    putchar('\n');
    linea('=');
    printf("  %s\n", texto);
    linea('=');
}

static void mostrar_banner(void)
{
    linea('=');
    printf("   ARTEMIS - SIMULADOR DE ASCENSO DEL SLS\n");
    printf("   Escenario 1: Ascenso y Propulsion (MRUV y Desprendimiento)\n");
    printf("   TPI Algoritmos y Estructuras de Datos - UTN\n");
    linea('=');
}

static void mostrar_menu(const Mision *m)
{
    printf("\n");
    linea('-');
    printf("  MENU PRINCIPAL   [simulacion: %s]\n",
           m->simulada ? "ejecutada" : "pendiente");
    linea('-');
    printf("   1. Configurar parametros de la mision\n");
    printf("   2. Ejecutar simulacion (Fase 1 -> Fase 2 -> Fase 3)\n");
    printf("   3. Ver reporte de desempeno\n");
    printf("   4. Ver parametros actuales\n");
    printf("   5. Restaurar parametros por defecto\n");
    printf("   6. Acerca del modelo fisico\n");
    printf("   0. Salir\n");
    linea('-');
}

/* ------------------------------ Parametros ------------------------------- */

static void mostrar_parametros(const Mision *m)
{
    const Parametros *p = &m->par;

    titulo("PARAMETROS ACTUALES");
    printf("  Separacion de SRB (fin de Fase 1)   : %8.1f s\n", p->t_srb);
    printf("  Throttle de los RS-25               : %8.1f %%\n", p->pct_empuje);
    printf("  Duracion de Fase 2 (nucleo solo)    : %8.1f s  (max %.1f s)\n",
           p->t_nucleo, tiempo_max_nucleo(p));
    if (p->hay_falla)
        printf("  Falla de motores en Fase 2          :       SI, a los %.1f s\n",
               p->t_falla);
    else
        printf("  Falla de motores en Fase 2          :       NO\n");
    printf("  Altura objetivo de apogeo           : %8.1f km\n",
           p->h_objetivo / 1000.0);
}

static void configurar_parametros(Mision *m)
{
    Parametros *p = &m->par;
    double tmax;

    titulo("CONFIGURACION DE LA MISION");

    printf("\n  --- Fase 1: SRB + nucleo ---\n");
    p->t_srb = leer_flotante("Instante de separacion de los SRB [s]",
                             T_SRB_MIN, T_COMB_SRB);

    printf("\n  --- Fase 2: nucleo solo ---\n");
    p->pct_empuje = leer_flotante("Throttle de los RS-25 [%]",
                                  EMPUJE_MIN_PCT, EMPUJE_MAX_PCT);

    tmax = tiempo_max_nucleo(p);
    printf("  (Con estos datos el nucleo puede quemar hasta %.1f s mas)\n", tmax);
    p->t_nucleo = leer_flotante("Duracion de la Fase 2 [s]", T_NUCLEO_MIN, tmax);

    printf("\n  --- Fase 3: fallo / caida libre ---\n");
    p->hay_falla = leer_si_no("Simular falla de motores durante la Fase 2?");
    if (p->hay_falla)
        p->t_falla = leer_flotante("Instante de la falla dentro de Fase 2 [s]",
                                   1.0, p->t_nucleo);
    else
        p->t_falla = 0.0;

    printf("\n  --- Objetivo ---\n");
    p->h_objetivo = 1000.0 * leer_flotante("Altura objetivo de apogeo [km]",
                                           H_OBJ_MIN_KM, H_OBJ_MAX_KM);

    reiniciar_mision(m);   /* los resultados anteriores ya no son validos */
    printf("\n  Parametros guardados. Ejecute la simulacion (opcion 2).\n");
}

/* ------------------------------ Reporte ---------------------------------- */

static const char *texto_resultado(Resultado r)
{
    switch (r) {
        case RES_CORTE_NOMINAL: return "Corte nominal (MECO) y trayectoria balistica";
        case RES_FALLA_CAIDA:   return "FALLA de motores - caida libre";
        case RES_ESCAPE:        return "Velocidad de escape superada";
        default:                return "Sin simular";
    }
}

static void mostrar_reporte(const Mision *m)
{
    int i;
    const Estado *s = &m->est_sep;
    const Estado *c = &m->est_corte;

    if (!m->simulada) {
        printf("\n  [!] Todavia no hay resultados. Ejecute la opcion 2.\n");
        return;
    }

    titulo("REPORTE DE DESEMPENO - MISION ARTEMIS (SLS)");

    /* ---- Tabla de eventos ---- */
    printf("\n  CRONOLOGIA DE EVENTOS\n");
    linea('-');
    printf("  %-27s %9s %10s %11s %9s %7s\n",
           "Evento", "t [s]", "h [km]", "v [m/s]", "masa [t]", "G");
    linea('-');
    for (i = 0; i < m->n_eventos; i++) {
        const Evento *ev = &m->eventos[i];
        printf("  %-27s %9.1f %10.2f %11.1f %9.1f %7.2f\n",
               ev->nombre, ev->est.t, ev->est.h / 1000.0, ev->est.v,
               ev->est.masa / 1000.0, ev->est.g_carga);
    }
    linea('-');
    printf("  G = carga sentida justo antes del evento (0 = sin empuje).\n");

    /* ---- Resumen por fase ---- */
    printf("\n  RESUMEN POR FASE\n");
    linea('-');
    printf("  Fase 1 (SRB + nucleo)   : %.1f s | altura %.2f km | vel. %.1f m/s (%.0f km/h)\n",
           s->t, s->h / 1000.0, s->v, s->v * 3.6);
    printf("  Fase 2 (nucleo solo)    : %.1f s | altura %.2f km | vel. %.1f m/s (%.0f km/h)\n",
           c->t - s->t, c->h / 1000.0, c->v, c->v * 3.6);
    printf("  Fase 3 (caida libre)    : apogeo %.2f km a los %.1f s | impacto a %.1f m/s\n",
           m->h_apogeo / 1000.0, m->t_apogeo, m->v_impacto);
    printf("  Propelente restante     : %.1f t de %.1f t en el nucleo\n",
           m->prop_nucleo / 1000.0, PROP_NUCLEO / 1000.0);
    printf("  Masa desprendida (SRB)  : %.1f t (casco seco + propelente no quemado)\n",
           N_SRB * (MASA_SRB_SECA + PROP_SRB * (1.0 - m->par.t_srb / T_COMB_SRB)) / 1000.0);
    linea('-');

    /* ---- Evaluacion ---- */
    printf("\n  EVALUACION FINAL\n");
    linea('-');
    printf("  Resultado        : %s\n", texto_resultado(m->resultado));
    printf("  Carga G maxima   : %.2f G  (limite tripulacion: %.1f G)\n",
           m->g_max, LIMITE_G_TRIPULACION);
    if (m->alerta_g)
        printf("  [ALERTA] Se supero el limite de carga G de la tripulacion.\n"
               "           Reduzca el throttle o acorte la Fase 2.\n");
    else
        printf("  Carga G dentro de los limites de seguridad.\n");

    if (m->resultado == RES_ESCAPE)
        printf("  Objetivo         : superado (la nave escapa del campo terrestre)\n");
    else
        printf("  Objetivo         : apogeo %.1f km vs. meta %.1f km -> %s\n",
               m->h_apogeo / 1000.0, m->par.h_objetivo / 1000.0,
               (m->h_apogeo >= m->par.h_objetivo) ? "ALCANZADO" : "NO ALCANZADO");
    linea('-');
}

/* ------------------------------ Otros ------------------------------------ */

static void acerca_modelo(void)
{
    titulo("ACERCA DEL MODELO FISICO");
    printf("  * Ascenso vertical, sin arrastre ni pitch-over (modelo ideal).\n");
    printf("  * Fases 1 y 2: MRUV. Aceleracion neta constante por fase:\n");
    printf("        a = Empuje / m_promedio - g(h_medio)\n");
    printf("        h = h0 + v0*t + a*t^2/2      v = v0 + a*t\n");
    printf("  * Gravedad variable: g(h) = G0 * (R / (R + h))^2\n");
    printf("  * Carga G sentida: G = Empuje / (masa * G0)\n");
    printf("  * Desprendimiento: se pierde casco seco + propelente no quemado\n");
    printf("    de los 2 SRB (91 t secas + 626 t de propelente c/u).\n");
    printf("  * Fase 3: sin empuje; integracion numerica (dt = %.2f s) hasta\n",
           DT_FASE3);
    printf("    el impacto. Si v >= v_escape, se informa escape.\n");
    printf("  * Datos aproximados del SLS Block 1; fines didacticos.\n");
}

static void ejecutar(Mision *m)
{
    titulo("EJECUTANDO SIMULACION");
    if (simular_mision(m)) {
        printf("  Fase 1 ... OK (SRB separados a los %.1f s)\n", m->par.t_srb);
        printf("  Fase 2 ... OK (%s)\n",
               m->par.hay_falla ? "con falla de motores" : "corte nominal");
        printf("  Fase 3 ... OK (%s)\n", texto_resultado(m->resultado));
        mostrar_reporte(m);
    }
}

/* ------------------------------ main ------------------------------------- */

int main(void)
{
    Mision mision;
    int opcion;

    inicializar_mision(&mision);
    mostrar_banner();

    do {
        mostrar_menu(&mision);
        opcion = leer_entero("Seleccione una opcion", 0, 6);

        switch (opcion) {
            case 1: configurar_parametros(&mision);               break;
            case 2: ejecutar(&mision);                            break;
            case 3: mostrar_reporte(&mision);                     break;
            case 4: mostrar_parametros(&mision);                  break;
            case 5: inicializar_mision(&mision);
                    printf("\n  Parametros restaurados.\n");      break;
            case 6: acerca_modelo();                              break;
            case 0: printf("\n  Fin del programa. Ad astra!\n"); break;
        }
        if (opcion != 0) esperar_enter();
    } while (opcion != 0);

    return EXIT_SUCCESS;
}
