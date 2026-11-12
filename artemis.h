/*
 * artemis.h
 * ---------------------------------------------------------------------------
 * TPI - Algoritmos y Estructuras de Datos (UTN)
 * Escenario 1: Ascenso y Propulsion con el SLS (MRUV y Desprendimiento)
 *
 * Contiene: constantes fisicas y del vehiculo, estructuras de datos y
 * prototipos de las funciones de simulacion.
 *
 * MODELO (simplificaciones declaradas):
 *   - Ascenso vertical (sin pitch-over), sin arrastre aerodinamico.
 *   - Cada fase se trata como MRUV: la aceleracion neta es constante dentro
 *     de la fase y se calcula con la masa PROMEDIO de la fase y la gravedad
 *     a la altura media (ley de gravitacion, g = G0 * (R/(R+h))^2).
 *   - Empuje constante en cada fase (el empuje real de los SRB decae al final).
 *   - La Fase 3 (caida libre) se integra numericamente con g variable.
 * ---------------------------------------------------------------------------
 */
#ifndef ARTEMIS_H
#define ARTEMIS_H

/* ======================= CONSTANTES FISICAS ============================== */
#define G0          9.80665        /* gravedad estandar [m/s^2]              */
#define R_TIERRA    6371000.0      /* radio medio terrestre [m]              */
#define MU_TIERRA   3.986004418e14 /* parametro gravitacional [m^3/s^2]      */

/* ======================= DATOS DEL VEHICULO (SLS Block 1) ================ */
#define N_SRB               2          /* cantidad de propulsores laterales  */
#define MASA_INICIAL        2608000.0  /* masa total al despegue [kg]        */
#define MASA_SRB_SECA       91000.0    /* masa seca de cada SRB [kg]         */
#define PROP_SRB            626000.0   /* propelente de cada SRB [kg]        */
#define EMPUJE_SRB          16.0e6     /* empuje de cada SRB [N]             */
#define T_COMB_SRB          126.0      /* duracion total de combustion [s]   */

#define N_RS25              4          /* motores RS-25 del nucleo           */
#define EMPUJE_RS25_NM      1.86e6     /* empuje por motor, nivel del mar [N]*/
#define EMPUJE_RS25_VAC     2.28e6     /* empuje por motor, vacio [N]        */
#define PROP_NUCLEO         987000.0   /* propelente del nucleo [kg]         */
#define FLUJO_NUCLEO_100    2056.0     /* caudal masico total al 100% [kg/s] */

/* Masa que queda sin contar el propelente del nucleo ni los SRB:
 * estructura del nucleo + etapa superior + Orion.                           */
#define MASA_RESTO_SECA  (MASA_INICIAL - N_SRB * (MASA_SRB_SECA + PROP_SRB) \
                          - PROP_NUCLEO)

/* ======================= LIMITES DE VALIDACION =========================== */
#define T_SRB_MIN           60.0       /* separacion de SRB, minimo [s]      */
#define EMPUJE_MIN_PCT      67.0       /* throttle minimo RS-25 [%]          */
#define EMPUJE_MAX_PCT      109.0      /* throttle maximo RS-25 [%]          */
#define T_NUCLEO_MIN        10.0       /* duracion minima Fase 2 [s]         */
#define H_OBJ_MIN_KM        100.0      /* altura objetivo minima [km]        */
#define H_OBJ_MAX_KM        5000.0     /* altura objetivo maxima [km]        */
#define LIMITE_G_TRIPULACION 4.0       /* limite de carga para la tripulacion*/

/* ======================= PARAMETROS DE SIMULACION ======================== */
#define MAX_EVENTOS         10
#define LONG_NOMBRE_EVENTO  40
#define DT_FASE3            0.05       /* paso de integracion Fase 3 [s]     */
#define MAX_ITER_FASE3      4000000L   /* tope de seguridad de iteraciones   */

/* ======================= ESTRUCTURAS DE DATOS ============================ */

/* Estado cinematico instantaneo de la nave. */
typedef struct {
    double t;        /* tiempo desde el despegue [s]                         */
    double h;        /* altura sobre la superficie [m]                       */
    double v;        /* velocidad vertical [m/s]                             */
    double masa;     /* masa actual [kg]                                     */
    double g_carga;  /* carga G sentida (empuje / (masa*G0)) [G]             */
} Estado;

/* Parametros que ingresa el usuario. */
typedef struct {
    double t_srb;       /* instante de separacion de los SRB [s]             */
    double pct_empuje;  /* throttle de los RS-25 [%]                         */
    double t_nucleo;    /* duracion de la Fase 2 (nucleo solo) [s]           */
    int    hay_falla;   /* 1 = simular falla de motores en Fase 2            */
    double t_falla;     /* instante de la falla dentro de la Fase 2 [s]      */
    double h_objetivo;  /* altura objetivo de apogeo [m]                     */
} Parametros;

/* Evento notable de la mision (fila del reporte). */
typedef struct {
    char   nombre[LONG_NOMBRE_EVENTO];
    Estado est;
} Evento;

/* Resultado final del vuelo. */
typedef enum {
    RES_SIN_SIMULAR = 0,
    RES_CORTE_NOMINAL,   /* MECO nominal y trayectoria balistica posterior   */
    RES_FALLA_CAIDA,     /* falla de motores y caida libre                   */
    RES_ESCAPE           /* velocidad >= velocidad de escape                 */
} Resultado;

/* Estado completo de la mision. */
typedef struct {
    Parametros par;
    Estado     est;                    /* estado actual                      */
    Estado     est_sep;                /* estado al separar los SRB          */
    Estado     est_corte;              /* estado al cortar el nucleo         */
    Evento     eventos[MAX_EVENTOS];
    int        n_eventos;
    double     prop_nucleo;            /* propelente restante del nucleo [kg]*/
    double     g_max;                  /* carga G maxima del vuelo [G]       */
    int        alerta_g;               /* 1 si se supero LIMITE_G_TRIPULACION*/
    double     h_apogeo;               /* altura del apogeo [m]              */
    double     t_apogeo;               /* instante del apogeo [s]            */
    double     v_impacto;              /* velocidad al impacto [m/s]         */
    Resultado  resultado;
    int        simulada;               /* 1 si ya se ejecuto la simulacion   */
} Mision;

/* ======================= PROTOTIPOS ====================================== */

/* Inicializacion */
void   inicializar_parametros(Parametros *p);
void   inicializar_mision(Mision *m);
void   reiniciar_mision(Mision *m);          /* conserva los parametros      */

/* Fisica auxiliar */
double gravedad_a_altura(double h);
double calcular_g_carga(double empuje, double masa);
double flujo_nucleo(double pct);
double empuje_nucleo_nm(double pct);
double empuje_nucleo_vac(double pct);
double tiempo_max_nucleo(const Parametros *p);
void   avanzar_mruv(Estado *e, double a, double dt);

/* Validacion de coherencia entre parametros (devuelve 1 si son validos) */
int    validar_parametros(const Parametros *p);

/* Fases del vuelo (devuelven 1 si terminan bien, 0 si hay error/escape) */
int    simular_fase1(Mision *m);   /* SRB + nucleo, hasta la separacion      */
int    simular_fase2(Mision *m);   /* nucleo solo, hasta MECO o falla        */
int    simular_fase3(Mision *m);   /* sin empuje: caida libre / balistica    */
int    simular_mision(Mision *m);  /* ejecuta las tres fases en orden        */

#endif /* ARTEMIS_H */
