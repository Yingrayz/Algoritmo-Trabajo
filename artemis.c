/*
 * artemis.c
 * ---------------------------------------------------------------------------
 * Implementacion de los calculos fisicos del Escenario 1 (SLS / Artemis).
 * Todas las funciones de estado reciben la estructura por PUNTERO.
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "artemis.h"

/* ========================================================================== */
/*  Inicializacion                                                            */
/* ========================================================================== */

void inicializar_parametros(Parametros *p)
{
    p->t_srb      = T_COMB_SRB;      /* separacion al agotarse los SRB       */
    p->pct_empuje = 100.0;           /* throttle nominal                     */
    p->t_nucleo   = 300.0;           /* duracion de Fase 2                   */
    p->hay_falla  = 0;
    p->t_falla    = 0.0;
    p->h_objetivo = 200000.0;        /* 200 km                               */
}

/* Pone en cero el estado de la mision pero conserva los parametros. */
void reiniciar_mision(Mision *m)
{
    Parametros aux = m->par;
    memset(m, 0, sizeof *m);
    m->par         = aux;
    m->prop_nucleo = PROP_NUCLEO;
    m->resultado   = RES_SIN_SIMULAR;
}

void inicializar_mision(Mision *m)
{
    inicializar_parametros(&m->par);
    reiniciar_mision(m);
}

/* ========================================================================== */
/*  Fisica auxiliar                                                           */
/* ========================================================================== */

/* g(h) = G0 * (R / (R + h))^2 */
double gravedad_a_altura(double h)
{
    double k = R_TIERRA / (R_TIERRA + h);
    return G0 * k * k;
}

/* Carga G sentida: aceleracion propia (empuje/masa) expresada en G0. */
double calcular_g_carga(double empuje, double masa)
{
    return empuje / masa / G0;
}

/* Caudal masico total del nucleo segun throttle (en %). */
double flujo_nucleo(double pct)
{
    return FLUJO_NUCLEO_100 * pct / 100.0;
}

double empuje_nucleo_nm(double pct)
{
    return N_RS25 * EMPUJE_RS25_NM * pct / 100.0;
}

double empuje_nucleo_vac(double pct)
{
    return N_RS25 * EMPUJE_RS25_VAC * pct / 100.0;
}

/* Maxima duracion posible de la Fase 2 segun el propelente que sobra
 * despues de la Fase 1 (el nucleo ya consumio durante t_srb segundos). */
double tiempo_max_nucleo(const Parametros *p)
{
    double flujo = flujo_nucleo(p->pct_empuje);
    return (PROP_NUCLEO - flujo * p->t_srb) / flujo;
}

/* Cinematica MRUV: h = h0 + v0*dt + a*dt^2/2 ; v = v0 + a*dt */
void avanzar_mruv(Estado *e, double a, double dt)
{
    e->h += e->v * dt + 0.5 * a * dt * dt;
    e->v += a * dt;
    e->t += dt;
}

/* Aceleracion neta constante de una fase (MRUV).
 * Se usa la masa promedio y la gravedad a la altura media (2 pasadas). */
static double aceleracion_neta(double empuje, double m_prom,
                               const Estado *e, double dt)
{
    double a1    = empuje / m_prom - gravedad_a_altura(e->h);
    double h_est = e->h + e->v * dt + 0.5 * a1 * dt * dt;
    double g_med = gravedad_a_altura(0.5 * (e->h + h_est));
    return empuje / m_prom - g_med;
}

/* Guarda un evento con el estado dado (si hay lugar en la tabla). */
static void registrar_evento(Mision *m, const char *nombre, const Estado *e)
{
    if (m->n_eventos >= MAX_EVENTOS) return;
    strncpy(m->eventos[m->n_eventos].nombre, nombre, LONG_NOMBRE_EVENTO - 1);
    m->eventos[m->n_eventos].nombre[LONG_NOMBRE_EVENTO - 1] = '\0';
    m->eventos[m->n_eventos].est = *e;
    m->n_eventos++;
}

static void actualizar_g_max(Mision *m, double g)
{
    if (g > m->g_max) m->g_max = g;
    if (g > LIMITE_G_TRIPULACION) m->alerta_g = 1;
}

/* ========================================================================== */
/*  Validacion de coherencia                                                  */
/* ========================================================================== */

int validar_parametros(const Parametros *p)
{
    double tmax;

    if (p->t_srb < T_SRB_MIN || p->t_srb > T_COMB_SRB) {
        printf("  [ERROR] Tiempo de separacion de SRB fuera de rango.\n");
        return 0;
    }
    if (p->pct_empuje < EMPUJE_MIN_PCT || p->pct_empuje > EMPUJE_MAX_PCT) {
        printf("  [ERROR] Throttle fuera de rango.\n");
        return 0;
    }
    tmax = tiempo_max_nucleo(p);
    if (p->t_nucleo < T_NUCLEO_MIN || p->t_nucleo > tmax) {
        printf("  [ERROR] La Fase 2 (%.1f s) excede el propelente disponible "
               "(maximo %.1f s con estos parametros).\n", p->t_nucleo, tmax);
        printf("          Reconfigure los parametros (opcion 1).\n");
        return 0;
    }
    if (p->hay_falla && (p->t_falla < 1.0 || p->t_falla > p->t_nucleo)) {
        printf("  [ERROR] El instante de falla debe estar dentro de la Fase 2.\n");
        return 0;
    }
    return 1;
}

/* ========================================================================== */
/*  FASE 1: SRB + nucleo (MRUV) y desprendimiento de los SRB                  */
/* ========================================================================== */

int simular_fase1(Mision *m)
{
    const Parametros *p = &m->par;
    Estado *e = &m->est;
    double pct    = p->pct_empuje;
    double dt     = p->t_srb;
    double empuje = N_SRB * EMPUJE_SRB + empuje_nucleo_nm(pct);
    double flujo  = N_SRB * PROP_SRB / T_COMB_SRB + flujo_nucleo(pct);
    double m_prom, a, g_ini, g_fin, m_desp, prop_srb_resto;

    /* --- Estado inicial: ignicion y despegue --- */
    e->t = 0.0;  e->h = 0.0;  e->v = 0.0;
    e->masa = MASA_INICIAL;
    g_ini = calcular_g_carga(empuje, e->masa);
    e->g_carga = g_ini;
    registrar_evento(m, "Ignicion y despegue", e);

    /* --- MRUV con masa promedio --- */
    m_prom = e->masa - flujo * dt / 2.0;
    a = aceleracion_neta(empuje, m_prom, e, dt);
    avanzar_mruv(e, a, dt);

    e->masa     -= flujo * dt;
    m->prop_nucleo -= flujo_nucleo(pct) * dt;

    g_fin = calcular_g_carga(empuje, e->masa);
    e->g_carga = g_fin;
    actualizar_g_max(m, g_ini);
    actualizar_g_max(m, g_fin);
    registrar_evento(m, "Fin de empuje de SRB", e);

    /* --- Desprendimiento: se pierden casco seco + propelente no quemado --- */
    prop_srb_resto = PROP_SRB * (1.0 - dt / T_COMB_SRB);
    m_desp = N_SRB * (MASA_SRB_SECA + prop_srb_resto);
    e->masa -= m_desp;

    e->g_carga = calcular_g_carga(empuje_nucleo_vac(pct), e->masa);
    m->est_sep = *e;
    registrar_evento(m, "Separacion de SRB", e);
    return 1;
}

/* ========================================================================== */
/*  FASE 2: nucleo solo (RS-25) hasta MECO o falla                            */
/* ========================================================================== */

int simular_fase2(Mision *m)
{
    const Parametros *p = &m->par;
    Estado *e = &m->est;
    double pct    = p->pct_empuje;
    double dt     = p->hay_falla ? p->t_falla : p->t_nucleo;
    double empuje = empuje_nucleo_vac(pct);
    double flujo  = flujo_nucleo(pct);
    double m_prom, a, g_ini, g_fin;

    g_ini = calcular_g_carga(empuje, e->masa);

    m_prom = e->masa - flujo * dt / 2.0;
    a = aceleracion_neta(empuje, m_prom, e, dt);
    avanzar_mruv(e, a, dt);

    e->masa        -= flujo * dt;
    m->prop_nucleo -= flujo * dt;

    g_fin = calcular_g_carga(empuje, e->masa);
    actualizar_g_max(m, g_ini);
    actualizar_g_max(m, g_fin);

    /* El evento registra la carga G JUSTO ANTES del corte. */
    e->g_carga = g_fin;
    m->est_corte = *e;
    registrar_evento(m, p->hay_falla ? "FALLA de motores (corte)"
                                     : "MECO (corte nominal)", e);

    e->g_carga = 0.0;      /* a partir de aca no hay empuje */
    return 1;
}

/* ========================================================================== */
/*  FASE 3: caida libre / trayectoria balistica (integracion numerica)        */
/* ========================================================================== */

int simular_fase3(Mision *m)
{
    Estado *e = &m->est;
    double v_esc2 = 2.0 * MU_TIERRA / (R_TIERRA + e->h);
    double v_prev, a;
    int    apogeo_ok = 0;
    long   it = 0;

    e->g_carga  = 0.0;
    m->h_apogeo = e->h;
    m->t_apogeo = e->t;

    /* Si la energia alcanza para escapar, no hay caida. */
    if (e->v * e->v >= v_esc2) {
        m->resultado = RES_ESCAPE;
        registrar_evento(m, "Supera velocidad de escape", e);
        return 0;
    }

    /* Integracion de Euler semi-implicito con g(h) variable. */
    while (e->h > 0.0 && it < MAX_ITER_FASE3) {
        v_prev = e->v;
        a = -gravedad_a_altura(e->h);
        e->v += a * DT_FASE3;
        e->h += e->v * DT_FASE3;
        e->t += DT_FASE3;
        it++;

        if (!apogeo_ok && v_prev > 0.0 && e->v <= 0.0) {
            apogeo_ok   = 1;
            m->h_apogeo = e->h;
            m->t_apogeo = e->t;
            registrar_evento(m, "Apogeo", e);
        }
    }

    if (e->h < 0.0) e->h = 0.0;
    m->v_impacto = fabs(e->v);
    registrar_evento(m, "Impacto", e);

    m->resultado = m->par.hay_falla ? RES_FALLA_CAIDA : RES_CORTE_NOMINAL;
    return 1;
}

/* ========================================================================== */
/*  Orquestador                                                               */
/* ========================================================================== */

int simular_mision(Mision *m)
{
    if (!validar_parametros(&m->par)) return 0;

    reiniciar_mision(m);                 /* limpia estado, conserva parametros */
    simular_fase1(m);
    simular_fase2(m);
    simular_fase3(m);
    m->simulada = 1;
    return 1;
}
