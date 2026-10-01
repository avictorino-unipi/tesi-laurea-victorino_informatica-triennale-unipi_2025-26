#include "estimators.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <arpa/inet.h>   /* INET6_ADDRSTRLEN */

/* Covariata individuale: sottorete del nodo, nota fin dalla prima osservazione e ancillare per i parametri. Codificata con indicatrici.
 * Si disattiva con -DHUG_USA_COVARIATE=0.
 */
#ifndef HUG_USA_COVARIATE
#define HUG_USA_COVARIATE 1
#endif

/* Numero massimo di categorie, riferimento incluso. Le eccedenti confluiscono nel riferimento. */
#ifndef HUG_MAX_CATEGORIE
#define HUG_MAX_CATEGORIE 12
#endif

/* Nodi minimi per categoria: un'indicatrice su un solo nodo produce separazione completa e un coefficiente divergente. */
#ifndef HUG_MIN_NODI_CATEGORIA
#define HUG_MIN_NODI_CATEGORIA 2
#endif

/* Repliche del bootstrap condizionale. */
#ifndef HUG_BOOT_REPLICHE
#define HUG_BOOT_REPLICHE 100
#endif

/* Modello candidato: quali blocchi del predittore lineare sono attivi. */
typedef struct {
    int time;           /* effetti-occasione (M_t) */
    int behav;          /* risposta comportamentale (M_b) */
    int cov;            /* covariate individuali (M_h) */
    const char *name;   /* etichetta */
} HugModello_t;

/* Coppia di modelli annidati per l'LRT. */
typedef struct {
    int nullo;      /* modello ristretto */
    int alt;        /* modello esteso */
} HugConfronto_t;

/* Esito di un candidato (campi validi solo se ok == 1). */
typedef struct {
    int p;          /* parametri stimati (s) */
    int ok;         /* stima usabile */
    double ll;      /* log-verosimiglianza condizionale */
    double aic;     /* criterio di Akaike */
    double Nhat;    /* N stimato */
} HugFit_t;

/* Buffer di lavoro di una stima (una sola allocazione e liberazione). */
typedef struct {
    unsigned char *occ_empty;   /* t */
    int *occ_par;               /* t */
    unsigned char *xh;          /* n * t */
    unsigned char *zh;          /* n * t */
    int *cat_of;                /* n */
    int *cat_cnt;               /* ncat */
    int *cat_par;               /* ncat */
    double *cov_use;            /* n * ncat */
} HugBuffers_t;

/* ===== Tabelle Modelli e Confronti ===== */

/* Modelli candidati. L'annidamento si ricava dai flag. */
static const HugModello_t HUG_MODELLI[7] = {
    { 0, 0, 0, "Modello 0  (M_0)"    },
    { 0, 1, 0, "Modello 1  (M_b)"    },
    { 1, 0, 0, "Modello 2  (M_t)"    },
    { 1, 1, 0, "Modello 3  (M_tb)"   },
    { 0, 0, 1, "Modello 4  (~M_h)"   },
    { 0, 1, 1, "Modello 5  (~M_bh)"  },
    { 1, 1, 1, "Modello 6  (~M_tbh)" }
};

/* Confronti LRT (nullo, alternativo), validati da hug_annidato(). */
static const HugConfronto_t HUG_CONFRONTI[7] = {
    { 0, 1 },
    { 0, 2 },
    { 0, 4 },
    { 1, 3 },
    { 1, 5 },
    { 4, 5 },
    { 5, 6 }
};

/* ===== Stato globale dello stimatore di Huggins ===== */

/* ---- Dati e dimensioni del problema ---- */
static int hug_n = 0;                   /* nodi osservati                        */
static int hug_t = 0;                   /* occasioni di cattura (incluse vuote)  */
static unsigned char *hug_x = NULL;     /* hug_n*hug_t: storia di cattura 0/1    */
static unsigned char *hug_z = NULL;     /* hug_n*hug_t: catturato in precedenza  */
static double *hug_cov = NULL;          /* hug_n*hug_n_cov: covariate per nodo   */

/* ---- Specificazione del modello corrente ---- */
static int hug_include_time = 0;        /* effetti-occasione (M_t)               */
static int hug_include_behav = 0;       /* risposta comportamentale (M_b)        */
static int hug_n_cov = 0;               /* numero di covariate individuali       */

/* ---- Disposizione del vettore beta, ricalcolata da hug_setup_layout() ---- */
static int hug_p = 0;                   /* numero totale di parametri            */
static int hug_ib = 0;                  /* posizione del coeff. comportamentale  */
static int hug_ic = 0;                  /* posizione della prima covariata       */
static int *hug_occ_par = NULL;         /* hug_t: indice in beta, -1 se assente  */

/* Occasioni vuote: restano nel campione, cosi' gli AIC sono confrontabili,
 * con probabilita' di cattura fissata a zero. */
static const unsigned char *hug_occ_empty = NULL;   /* hug_t: 1 se occasione vuota */

/* ---- Stato xorshift del bootstrap (indipendente da rand()) ---- */
static uint64_t hug_rng_state = 88172645463325252ULL;

static double factorial(int n) {
    double result = 1.0;
    for (int i = 1; i <= n; i++) {
        result *= (double)i;
    }
    return result;
}

/* Coda superiore della funzione gamma incompleta regolarizzata. */
static double gamma_q(double a, double x) {
    if (a <= 0.0) return 1.0;
    if (x <= 0.0) return 1.0;

    if (x < a + 1.0) {
        /* Sviluppo in serie, per x piccolo rispetto ad a. */
        double ap = a;
        double sum = 1.0 / a;
        double del = sum;
        for (int i = 1; i <= 1000; i++) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (fabs(del) < fabs(sum) * 1e-15) break;
        }
        double p = sum * exp(-x + a * log(x) - lgamma(a));
        return 1.0 - p;
    } else {
        /* Frazione continua di Lentz, per x grande. */
        const double tiny = 1e-300;
        double b = x + 1.0 - a;
        double c = 1.0 / tiny;
        double d = 1.0 / b;
        double h = d;
        for (int i = 1; i <= 1000; i++) {
            double an = -(double)i * ((double)i - a);
            b += 2.0;
            d = an * d + b;
            if (fabs(d) < tiny) {
                d = tiny;
            }
            c = b + an / c;
            if (fabs(c) < tiny) {
                c = tiny;
            }
            d = 1.0 / d;
            double del = d * c;
            h *= del;
            if (fabs(del - 1.0) < 1e-15) break;
        }
        return exp(-x + a * log(x) - lgamma(a)) * h;
    }
}

/* Probabilita' che una chi-quadro con df gradi di liberta' superi x. */
static double chi2_sf(double x, int df) {
    if (df <= 0) return 1.0;
    if (x <= 0.0) return 1.0;
    return gamma_q(0.5 * (double)df, 0.5 * x);
}

/* ===== Stimatore Jackknife ===== */

/* IC 95% simmetrico, con l'estremo inferiore mai sotto il numero di nodi osservati. */
static const char *fmt_ic95(char *buf, size_t bufsz, double N, double se, double s_obs) {
    double lo = N - (1.96 * se);
    double hi = N + (1.96 * se);
    if (lo < s_obs) {
        lo = s_obs;
    }
    snprintf(buf, bufsz, "[%.2f, %.2f]", lo, hi);
    return buf;
}

void node_jackknife_estimators_print_stats(const EstimatorData_t *d) {
    double s_obs = (double)d->n_nodes;
    double t = (double)d->t;
    int f1 = 0;  /* nodi visti 1 volta  */
    int f2 = 0;  /* nodi visti 2 volte  */
    int f3 = 0;  /* nodi visti 3 volte  */
    int f4 = 0;  /* nodi visti 4 volte  */
    int f5 = 0;  /* nodi visti 5 volte  */
    int fn = 0;  /* nodi visti piu' di 5 volte (coefficiente unitario) */

    for (int i = 0; i < d->n_nodes; i++) {
        int seen_count = 0;
        for (int c = 0; c < d->t; c++) {
            seen_count += d->seen[i][c];
        }
        if (seen_count == 1) {
            f1++;
        } else if (seen_count == 2) {
            f2++;
        } else if (seen_count == 3) {
            f3++;
        } else if (seen_count == 4) {
            f4++;
        } else if (seen_count == 5) {
            f5++;
        } else if (seen_count > 5) {
            fn++;
        }
    }

    fprintf(stdout, "\n=============== STATISTICHE SULLE FREQUENZE DEI NODI ===============\n");
    fprintf(stdout, "Occasioni di cattura (t): %d\n", d->t);
    fprintf(stdout, "Nodi unici totali osservati (S_obs): %d\n\n", d->n_nodes);
    fprintf(stdout, "Nodi visti in una sola cattura (f1): %d\n", f1);
    fprintf(stdout, "Nodi visti in due sole catture (f2): %d\n", f2);
    fprintf(stdout, "Nodi visti in tre sole catture (f3): %d\n", f3);
    fprintf(stdout, "Nodi visti in quattro sole catture (f4): %d\n", f4);
    fprintf(stdout, "Nodi visti in cinque sole catture (f5): %d\n", f5);
    fprintf(stdout, "Nodi visti in piu' di cinque catture: %d\n", fn);

    if (d->t < 2 || s_obs < 2.0) {
        fprintf(stdout, "\nOccasioni o nodi insufficienti (t=%d, S_obs=%.0f): stimatore non calcolabile.\n", d->t, s_obs);
        return;
    }

    /* Lo stimatore e' studiato fra 5 e 30 occasioni: con meno di 5 le stime sono indicative. */
    if (t < 5.0) {
        fprintf(stdout, "\nATTENZIONE: t=%.0f < 5. Burnham & Overton (1979) studiano lo stimatore per\n", t);
        fprintf(stdout, "5 <= t <= 30: con meno occasioni le stime vanno considerate indicative.\n");
    }

    /* Coefficienti di correzione di ciascun ordine, dove l'ordine k richiede almeno k occasioni. */
    int kmax = d->t;
    if (kmax > 5) {
        kmax = 5;
    }

    double f[6] = { 0.0, (double)f1, (double)f2, (double)f3, (double)f4, (double)f5 };
    double a[6][6];      /* coefficienti completi, 1 oltre l'ordine */
    double alpha[6][6];  /* coefficienti di correzione */

    for (int k = 1; k <= 5; k++) {
        for (int i = 1; i <= 5; i++) {
            a[k][i] = 1.0;
            alpha[k][i] = 0.0;
        }
    }

    if (kmax >= 1) {
        alpha[1][1] = (t - 1.0) / t;
    }
    if (kmax >= 2) {
        alpha[2][1] = ((2.0 * t) - 3.0) / t;
        alpha[2][2] = -(((t - 2.0) * (t - 2.0)) / (t * (t - 1.0)));
    }
    if (kmax >= 3) {
        alpha[3][1] = ((3.0 * t) - 6.0) / t;
        alpha[3][2] = -(((3.0 * t * t) - (15.0 * t) + 19.0) / (t * (t - 1.0)));
        alpha[3][3] = (((t - 3.0) * (t - 3.0) * (t - 3.0)) / (t * (t - 1.0) * (t - 2.0)));
    }
    if (kmax >= 4) {
        alpha[4][1] = ((4.0 * t) - 10.0) / t;
        alpha[4][2] = -(((6.0 * t * t) - (36.0 * t) + 55.0) / (t * (t - 1.0)));
        alpha[4][3] = (((4.0 * t * t * t) - (42.0 * t * t) + (148.0 * t) - 175.0) / (t * (t - 1.0) * (t - 2.0)));
        alpha[4][4] = -(((t - 4.0) * (t - 4.0) * (t - 4.0) * (t - 4.0)) / (t * (t - 1.0) * (t - 2.0) * (t - 3.0)));
    }
    if (kmax >= 5) {
        alpha[5][1] = ((5.0 * t) - 15.0) / t;
        alpha[5][2] = -(((10.0 * t * t) - (70.0 * t) + 125.0) / (t * (t - 1.0)));
        alpha[5][3] = (((10.0 * t * t * t) - (120.0 * t * t) + (485.0 * t) - 660.0) / (t * (t - 1.0) * (t - 2.0)));
        alpha[5][4] = -(((((t - 4.0) * (t - 4.0) * (t - 4.0) * (t - 4.0) * (t - 4.0)) - ((t - 5.0) * (t - 5.0) * (t - 5.0) * (t - 5.0) * (t - 5.0)))) / (t * (t - 1.0) * (t - 2.0) * (t - 3.0)));
        alpha[5][5] = (((t - 5.0) * (t - 5.0) * (t - 5.0) * (t - 5.0) * (t - 5.0)) / (t * (t - 1.0) * (t - 2.0) * (t - 3.0) * (t - 4.0)));
    }

    for (int k = 1; k <= kmax; k++) {
        for (int i = 1; i <= k; i++) {
            a[k][i] = alpha[k][i] + 1.0;
        }
    }

    /* Stima di ogni ordine: nodi osservati piu' le frequenze pesate dai
     * coefficienti di correzione. La varianza usa i coefficienti completi. */
    double Nj[6] = { 0.0 };
    double var[6] = { 0.0 };
    double se[6] = { 0.0 };
    int var_neg[6] = { 0 };
    int below_s[6] = { 0 };
    Nj[0] = s_obs;

    for (int k = 1; k <= kmax; k++) {
        double N = s_obs;
        for (int i = 1; i <= k; i++) {
            N += alpha[k][i] * f[i];
        }
        Nj[k] = N;

        if (N < s_obs) {
            below_s[k] = 1;
        }

        double sum = 0.0;
        for (int i = 1; i <= 5; i++) {
            sum += a[k][i] * a[k][i] * f[i];
        }
        sum += (double)fn;  /* coefficiente unitario oltre la quinta frequenza */
        var[k] = sum - N;
        if (var[k] < 0.0) {
            var_neg[k] = 1;
            var[k] = 0.0;
        }
        se[k] = sqrt(var[k]);
    }

    char icbuf[64];   /* buffer per fmt_ic95() */

    fprintf(stdout, "\n=============== CALCOLO STIMATORE DI JACKKNIFE ===============\n");
    fprintf(stdout, "Ordine | N stimato  | Err.Std   | IC 95%% (troncato a S_obs)\n");
    fprintf(stdout, "------------------------------------------------------------------\n");
    for (int k = 1; k <= 5; k++) {
        if (k > kmax) {
            fprintf(stdout, "  J%d   | %10s | %9s | non calcolabile (richiede t >= %d, qui t = %.0f)\n", k, "-", "-", k, t);
            continue;
        }
        fprintf(stdout, "  J%d   | %10.2f | %9.2f | %s%s%s\n", k, Nj[k], se[k], fmt_ic95(icbuf, sizeof(icbuf), Nj[k], se[k], s_obs),
                var_neg[k] ? "   [varianza stimata negativa, azzerata]" : "",
                below_s[k] ? "   [N_Jk < S_obs: stima non utilizzabile]" : "");
    }

    /* Test sequenziale: si verifica se passare all'ordine successivo cambia la
     * stima in modo significativo. Il test e' condizionato ai nodi osservati. */
    double Pk[6] = { 0.0 };
    double Tk[6] = { 0.0 };
    int have_test[6] = { 0 };

    fprintf(stdout, "\n--- Test sequenziale per la scelta dell'ordine ---\n");
    fprintf(stdout, "Confronto | T_k      | P_k     | Esito\n");
    for (int k = 1; k <= kmax - 1; k++) {
        double diff = Nj[k + 1] - Nj[k];
        double sum_b2f = 0.0;
        for (int i = 1; i <= 5; i++) {
            double bi = a[k + 1][i] - a[k][i];
            sum_b2f += bi * bi * f[i];
        }
        double var_diff = (s_obs / (s_obs - 1.0)) * (sum_b2f - ((diff * diff) / s_obs));

        if (var_diff > 0.0) {
            Tk[k] = diff / sqrt(var_diff);
            Pk[k] = erfc(fabs(Tk[k]) / sqrt(2.0));   /* livello di significativita' bilaterale */
            have_test[k] = 1;
            fprintf(stdout, "  H0%d->%d  | %8.3f | %7.4f | %s\n", k, k + 1, Tk[k], Pk[k], (Pk[k] > 0.05) ? "NON rifiutata -> STOP" : "rifiutata -> continua");
        } else {
            /* Varianza non positiva: test non disponibile, e non trattato come accettato. */
            have_test[k] = 0;
            fprintf(stdout, "  H0%d->%d  | %8s | %7s | non disponibile (varianza stimata <= 0)\n", k, k + 1, "-", "-");
        }
    }

    int ksel = 0;
    for (int k = 1; k <= kmax - 1 && ksel == 0; k++) {
        if (have_test[k] && Pk[k] > 0.05) {
            ksel = k;
        }
    }

    int fallback = 0;
    if (ksel == 0) {
        /* Tutte le ipotesi rifiutate o nessun test disponibile, e la scelta andrebbe fatta fra i primi tre ordini.
        *  Per convenzione si usa il primo, a varianza minima. 
        */
        int alcun_test = 0;
        for (int k = 1; k <= kmax - 1; k++) {
            if (have_test[k]) {
                alcun_test = 1;
            }
        }

        if (alcun_test) {
            fprintf(stdout, "\nATTENZIONE: tutte le ipotesi H0k disponibili sono state RIFIUTATE. Il rifiuto\n");
            fprintf(stdout, "indica una distribuzione delle probabilita' di cattura molto dispersa, per la\n");
            fprintf(stdout, "quale l'intera famiglia jackknife fornisce stime instabili.\n");
        } else {
            fprintf(stdout, "\nATTENZIONE: NESSUN test e' risultato disponibile (varianza stimata della\n");
            fprintf(stdout, "differenza non positiva). Non e' un'indicazione di eterogeneita': segnala\n");
            fprintf(stdout, "insufficienza dei dati (pochi nodi o frequenze concentrate).\n");
        }
        fprintf(stdout, "Burnham & Overton (1979) raccomandano in questo caso una scelta manuale\n");
        fprintf(stdout, "fra J1, J2 e J3. Si adotta per convenzione J1, l'ordine a varianza minore.\n");
        ksel = 1;
        fallback = 1;
    }

    fprintf(stdout, "\nOrdine Jackknife selezionato: J%d (Nj=%.2f, se=%.2f, IC95%%=%s)%s%s\n", ksel, Nj[ksel], se[ksel], fmt_ic95(icbuf, sizeof(icbuf), Nj[ksel], se[ksel], s_obs),
            fallback ? "   [convenzione: nessun test accettato]" : "",
            below_s[ksel] ? "   [N_Jk < S_obs: stima non utilizzabile]" : "");

    /* Stimatore interpolato: media pesata fra i due ordini adiacenti, con peso dato da quanto la soglia del 5% e' vicina ai rispettivi livelli di significativita'. */
    double Nint = Nj[1];
    double se_int = se[1];
    double c = 1.0;

    if (ksel == 1) {
        /* Ordine 1: nessuna interpolazione. */
        fprintf(stdout, "Stimatore Jackknife interpolato: non applicabile (ordine 1).\n");
    } else if (have_test[ksel - 1]) {
        double denom = Pk[ksel] - Pk[ksel - 1];
        if (fabs(denom) > 1e-12) {
            c = (0.05 - Pk[ksel - 1]) / denom;
            if (c < 0.0) {
                c = 0.0;
            }
            if (c > 1.0) {
                c = 1.0;
            }

            double dcoef[6];
            for (int i = 1; i <= 5; i++) {
                dcoef[i] = (c * a[ksel][i]) + ((1.0 - c) * a[ksel - 1][i]);
            }

            Nint = 0.0;
            for (int i = 1; i <= 5; i++) {
                Nint += dcoef[i] * f[i];
            }
            Nint += (double)fn; /* coefficiente unitario oltre la quinta frequenza */

            double sum = 0.0;
            for (int i = 1; i <= 5; i++) {
                sum += dcoef[i] * dcoef[i] * f[i];
            }
            sum += (double)fn;
            double v = sum - Nint;
            se_int = sqrt(v > 0.0 ? v : 0.0);
        } else {
            Nint = Nj[ksel];
            se_int = se[ksel];
        }
    } else {
        Nint = Nj[ksel];
        se_int = se[ksel];
    }

    if (ksel > 1) {
        fprintf(stdout, "Stimatore Jackknife interpolato: %.2f (se=%.2f, IC95%%=%s, c=%.4f)%s\n", Nint, se_int, fmt_ic95(icbuf, sizeof(icbuf), Nint, se_int, s_obs), c,
                (Nint < s_obs) ? "   [N_J < S_obs: stima non utilizzabile]" : "");
    }
}

/* ===== Stimatore di Chao ===== */

/* Varianza della stima di Chao e intervalli simmetrico e log-trasformato. */
static void chao_confidence_interval(int s_obs, double chao, int f1, int f2) {
    double r  = (double)f1 / (double)f2;
    double r2 = r * r;
    double r3 = r2 * r;
    double r4 = r3 * r;
    double var = (double)f2 * ((0.25 * r4) + r3 + (0.5 * r2));
    double std_err = sqrt(var);
    double diff = chao - (double)s_obs;

    /* Se l'intervallo simmetrico viene troncato, e' preferibile il log-trasformato. */
    int ci_troncato = ((chao - (1.96 * std_err)) < (double)s_obs);

    double log_term = log(1.0 + (var / (diff * diff)));
    double C = exp(1.96 * sqrt(log_term));
    double ci_lower_log = (double)s_obs + (diff / C);
    double ci_upper_log = (double)s_obs + (diff * C);

    char icbuf[64];   /* buffer per fmt_ic95() */

    fprintf(stdout, "\nNOTA: l'intervallo di fiducia si riferisce alla stima della eq. (8).\n\n");
    fprintf(stdout, "Varianza stimata: %.4f (Dev.Std: %.2f)\n", var, std_err);
    fprintf(stdout, "Intervallo di fiducia 95%% (simmetrico eq. 11, troncato a S_obs): %s%s\n", fmt_ic95(icbuf, sizeof(icbuf), chao, std_err, (double)s_obs), ci_troncato ? "   [estremo inferiore troncato]" : "");
    fprintf(stdout, "Intervallo di fiducia 95%% (log-trasformato, eq. 12): [%.2f, %.2f]\n", ci_lower_log, ci_upper_log);
    if (ci_troncato) {
        fprintf(stdout, "Con l'estremo inferiore troncato e' preferibile l'intervallo log-trasformato,\n");
        fprintf(stdout, "che rispetta il vincolo N >= S per costruzione.\n");
    }
}

void node_chao_estimators_print_stats(const EstimatorData_t *d) {
    double s_obs = (double)d->n_nodes;
    double t = (double)d->t;
    int f1 = 0;  /* nodi visti 1 volta  */
    int f2 = 0;  /* nodi visti 2 volte  */
    int f3 = 0;  /* nodi visti 3 volte  */

    for (int i = 0; i < d->n_nodes; i++) {
        int seen_count = 0;
        for (int cc = 0; cc < d->t; cc++) {
            seen_count += d->seen[i][cc];
        }
        if (seen_count == 1) {
            f1++;
        } else if (seen_count == 2) {
            f2++;
        } else if (seen_count == 3) {
            f3++;
        }
    }

    fprintf(stdout, "\n=============== STIMATORE DI CHAO (1987) ===============\n");

    /* L'approssimazione poissoniana richiede molte occasioni (almeno 5). */
    if (t < 5.0) {
        fprintf(stdout, "ATTENZIONE: t=%.0f < 5. Chao (1987) raccomanda almeno 5 occasioni di cattura\n", t);
        fprintf(stdout, "perche' valga l'approssimazione poissoniana: la stima e' solo indicativa.\n\n");
    }

    /* La stima richiede almeno un nodo visto una volta e uno visto due volte. */
    if (f1 == 0 || f2 == 0) {
        fprintf(stdout, "Stima eq. (8): NULL [non definita con f1=%d, f2=%d]\n", f1, f2);
        fprintf(stdout, "Limite inferiore eq. (9): NULL [momenti non calcolabili]\n");
        fprintf(stdout, "Approssimazione di Cormack eq. (10): NULL [momenti non calcolabili]\n");
        return;
    }

    /* Momenti stimati dai rapporti fra frequenze consecutive. */
    double m1 = factorial(1 + 1) * ((double)f2 / (double)f1);
    double m2 = factorial(2 + 1) * ((double)f3 / (double)f1);

    /* Stima di riferimento: varianza e intervalli si riferiscono a questa. */
    double chao = s_obs + (((double)f1 * (double)f1) / (2.0 * (double)f2));

    fprintf(stdout, "Stima eq. (8) [N_Ch]: %.2f nodi totali stimati\n", chao);

    /* I momenti devono essere compatibili con una distribuzione sull'intervallo da zero a t, altrimenti i raffinamenti non sono calcolabili. */
    int moments_ok = ((t * t) > (t * m1)) && ((t * m1) > m2) && (m2 > (m1 * m1));
    if (moments_ok) {
        double term1 = (double)f1 / (((t - m1) * (t - m1)) + m2 - (m1 * m1));
        double term2 = ((t - m1) * (t - m1) * (t - m1)) / ((t * m1) - m2);
        double term3 = (m2 - (m1 * m1)) / t;

        double chao_min = s_obs + (term1 * (term2 + term3));
        double chao_cormack = s_obs + ((((double)f1 * (double)f1) / (2.0 * (double)f2)) * ((1.0 - (m1 / t)) / (1.0 - (m2 / (t * m1)))));

        fprintf(stdout, "Limite inferiore eq. (9) [N_min]: %.2f nodi totali stimati\n", chao_min);
        fprintf(stdout, "Approssimazione di Cormack eq. (10) [N_Co]: %.2f nodi totali stimati\n", chao_cormack);
    } else {
        fprintf(stdout, "Limite inferiore eq. (9): NULL [momenti non legittimi in [0,t]]\n");
        fprintf(stdout, "Approssimazione di Cormack eq. (10): NULL [momenti non legittimi in [0,t]]\n");
    }

    /* Stima e intervalli si riferiscono alla stessa quantita'. */
    fprintf(stdout, "\nSi stimano circa %.0f nodi attivi nella rete, di cui:\n", chao);
    fprintf(stdout, " - Osservati direttamente: %d\n", d->n_nodes);
    fprintf(stdout, " - Potenzialmente nascosti: circa %.0f\n", chao - s_obs);

    chao_confidence_interval(d->n_nodes, chao, f1, f2);
}

/* ===== Probabilita' di cattura per sottorete (diagnostica) ===== */

void node_subnet_capture_probability_print_stats(const EstimatorData_t *d) {
    if (d->t <= 0 || d->n_subnets == 0) return;

    int *seen_per_capture = calloc((size_t)d->n_subnets * (size_t)d->t, sizeof(int));
    int *subnet_node_count = calloc((size_t)d->n_subnets, sizeof(int));
    double *subnet_total_seen = calloc((size_t)d->n_subnets, sizeof(double));

    if (seen_per_capture == NULL || subnet_node_count == NULL || subnet_total_seen == NULL) {
        fprintf(stderr, "Attenzione: allocazione memoria fallita in node_subnet_capture_probability_print_stats().\n");
        free(seen_per_capture);
        free(subnet_node_count);
        free(subnet_total_seen);
        return;
    }

    /* Ogni nodo (identita' = MAC) appartiene a una sola sottorete. */
    for (int i = 0; i < d->n_nodes; i++) {
        int s = d->subnet_idx[i];
        if (s < 0 || s >= d->n_subnets) {
            continue;
        }

        subnet_node_count[s]++;
        for (int cc = 0; cc < d->t; cc++) {
            if (d->seen[i][cc]) {
                subnet_total_seen[s] += 1.0;
                seen_per_capture[(s * d->t) + cc]++;
            }
        }
    }

    fprintf(stdout, "\n=============== PROBABILITA' DI CATTURA PER SOTTORETE ===============\n");
    fprintf(stdout, "NOTA: P(capture) di un nodo = (catture in cui e' stato visto) / (catture totali = %d)\n", d->t);
    fprintf(stdout, "NOTA: P(capture) di una sottorete = media delle P(capture) dei suoi nodi\n\n");

    for (int s = 0; s < d->n_subnets; s++) {
        /* sottorete vuota: non dovrebbe accadere */
        if (subnet_node_count[s] == 0) continue;

        fprintf(stdout, "--- Sottorete %s (%s, %d nodi) ---\n", d->subnet_name[s], d->subnet_version[s] == 6 ? "IPv6" : "IPv4", subnet_node_count[s]);

        for (int cc = 0; cc < d->t; cc++) {
            int count_c = seen_per_capture[(s * d->t) + cc];
            double prob_c = (double)count_c / (double)subnet_node_count[s];
            fprintf(stdout, "  Cattura %-2d: %3d/%-3d nodi visti  ->  P(capture) = %.4f\n", cc + 1, count_c, subnet_node_count[s], prob_c);
        }

        double avg_p = subnet_total_seen[s] / ((double)subnet_node_count[s] * (double)d->t);
        fprintf(stdout, "  %-15s P(capture) media = %.4f\n", "Media su tutte le catture:", avg_p);
        fprintf(stdout, "\n");
    }

    fprintf(stdout, "=============== PROBABILITA' DI CATTURA MEDIA PER SOTTORETE ===============\n");
    fprintf(stdout, "%-24s | %-5s | %-42s | %-10s | %-10s | %-15s\n",
            "Sottorete", "IPv", "Range", "Nodi", "Flussi", "P(capture) media");
    fprintf(stdout, "--------------------------------------------------------------------------------------------------------------------------\n");
    for (int s = 0; s < d->n_subnets; s++) {
        if (subnet_node_count[s] == 0) continue;

        /* Range: primo indirizzo (dalla stringa "rete/prefisso") e ultimo. */
        char first_addr[INET6_ADDRSTRLEN];
        const char *slash = strchr(d->subnet_name[s], '/');

        size_t first_len = slash ? (size_t)(slash - d->subnet_name[s]) : strlen(d->subnet_name[s]);
        if (first_len >= sizeof(first_addr)) {
            first_len = sizeof(first_addr) - 1;
        }
        memcpy(first_addr, d->subnet_name[s], first_len);
        first_addr[first_len] = '\0';

        char range_buf[(2 * INET6_ADDRSTRLEN) + 4];
        snprintf(range_buf, sizeof(range_buf), "%s - %s", first_addr, d->subnet_range_end[s]);

        double avg_p = subnet_total_seen[s] / ((double)subnet_node_count[s] * (double)d->t);
        fprintf(stdout, "%-24s | %-5s | %-42s | %-10d | %-10d | %.4f\n",
                d->subnet_name[s],
                d->subnet_version[s] == 6 ? "IPv6" : "IPv4",
                range_buf,
                subnet_node_count[s],
                d->subnet_flow_count[s],
                avg_p);
    }

    free(seen_per_capture);
    free(subnet_node_count);
    free(subnet_total_seen);
}

/* ===== Stimatore di Huggins con verosimiglianza condizionale ===== */

/* "a" e' annidato in "b" se ogni blocco attivo in "a" lo e' anche in "b". */
static int hug_annidato(int a, int b) {
    if (a == b) return 0;
    if (HUG_MODELLI[a].time && !HUG_MODELLI[b].time) return 0;
    if (HUG_MODELLI[a].behav && !HUG_MODELLI[b].behav) return 0;
    if (HUG_MODELLI[a].cov && !HUG_MODELLI[b].cov) return 0;
    return 1;
}

/* Nome di una categoria. L'ultimo indice e' "nessuna sottorete". */
static const char *hug_nome_cat(const EstimatorData_t *d, int cc) {
    if (cc >= 0 && cc < d->n_subnets) return d->subnet_name[cc];
    return "(nessuna sottorete)";
}

/* Occasione vuota "congelata": probabilita' di cattura nulla in ogni modello, cosi' non abbassa l'intercetta (gonfiando N) e gli AIC restano confrontabili. */
static int hug_frozen(int j) {
    if (hug_occ_empty == NULL) return 0;
    return hug_occ_empty[j] ? 1 : 0;
}

/* Associa ogni occasione al suo parametro e ricalcola hug_p. L'ultima occasione attiva fa da riferimento, con coefficiente nullo. */
static void hug_setup_layout(void) {
    int npar_time = 0;

    if (hug_include_time) {
        int last_active = -1;
        for (int j = 0; j < hug_t; j++) {
            if (!hug_occ_empty[j]) {
                last_active = j;
            }
        }
        int idx = 1;
        for (int j = 0; j < hug_t; j++) {
            if (hug_occ_empty[j] || j == last_active) {
                hug_occ_par[j] = -1;
            } else {
                hug_occ_par[j] = idx;
                idx++;
            }
        }
        npar_time = idx - 1;
    } else {
        for (int j = 0; j < hug_t; j++) {
            hug_occ_par[j] = -1;
        }
    }

    hug_ib = 1 + npar_time;
    hug_ic = hug_ib + (hug_include_behav ? 1 : 0);
    hug_p = hug_ic + hug_n_cov;
}

/* ---- Sigmoide numericamente stabile ---- */
static double hug_sigmoid(double x) {
    if (x >= 0.0) {
        double z = exp(-x);
        return 1.0 / (1.0 + z);
    } else {
        double z = exp(x);
        return z / (1.0 + z);
    }
}

/* Inversa di una matrice n x n (Gauss-Jordan con pivoting parziale), ritorna 0 se singolare. */
static int hug_invert(const double *A_in, int n, double *Ainv) {
    double *A = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (A == NULL) return 0;

    for (int i = 0; i < n * n; i++) {
        A[i] = A_in[i];
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            Ainv[(i * n) + j] = (i == j) ? 1.0 : 0.0;
        }
    }

    for (int col = 0; col < n; col++) {
        int piv = col;
        double best = fabs(A[(col * n) + col]);
        for (int r = col + 1; r < n; r++) {
            double v = fabs(A[(r * n) + col]);
            if (v > best) {
                best = v;
                piv = r;
            }
        }

        if (best < 1e-14) {
            free(A);
            return 0;
        }

        if (piv != col) {
            for (int k = 0; k < n; k++) {
                double tmp = A[(col * n) + k];
                A[(col * n) + k] = A[(piv * n) + k];
                A[(piv * n) + k] = tmp;
                tmp = Ainv[(col * n) + k];
                Ainv[(col * n) + k] = Ainv[(piv * n) + k];
                Ainv[(piv * n) + k] = tmp;
            }
        }

        double dg = A[(col * n) + col];
        for (int k = 0; k < n; k++) {
            A[(col * n) + k] /= dg;
            Ainv[(col * n) + k] /= dg;
        }
        for (int r = 0; r < n; r++) {
            if (r == col) continue;
            double fct = A[(r * n) + col];
            if (fct == 0.0) continue;
            for (int k = 0; k < n; k++) {
                A[(r * n) + k] -= fct * A[(col * n) + k];
                Ainv[(r * n) + k] -= fct * Ainv[(col * n) + k];
            }
        }
    }
    free(A);
    return 1;
}

/* Vettore di disegno di un nodo in un'occasione. Con use_z=0 si ignora la
 * cattura pregressa (probabilita' di prima cattura). Nullo per le occasioni congelate. */
static void hug_design(int i, int j, int use_z, double *w) {
    for (int r = 0; r < hug_p; r++) {
        w[r] = 0.0;
    }
    if (hug_frozen(j)) return;

    w[0] = 1.0;
    if (hug_occ_par[j] >= 0) {
        w[hug_occ_par[j]] = 1.0;
    }
    if (hug_include_behav) {
        w[hug_ib] = use_z ? (double)hug_z[(i * hug_t) + j] : 0.0;
    }
    for (int cc = 0; cc < hug_n_cov; cc++) {
        w[hug_ic + cc] = hug_cov[(i * hug_n_cov) + cc];
    }
}

/* Predittore lineare con cattura pregressa esplicita (serve al bootstrap), limitato a [-30, 30]. */
static double hug_eta_z(const double *beta, int i, int j, int zval) {
    /* occasione congelata: probabilita' praticamente nulla */
    if (hug_frozen(j)) return -30.0;   

    double eta = beta[0];
    if (hug_occ_par[j] >= 0) {
        eta += beta[hug_occ_par[j]];
    }
    if (hug_include_behav && zval) {
        eta += beta[hug_ib];
    }
    for (int cc = 0; cc < hug_n_cov; cc++) {
        eta += beta[hug_ic + cc] * hug_cov[(i * hug_n_cov) + cc];
    }
    if (eta > 30.0) {
        eta = 30.0;     /* anti-overflow */
    }
    if (eta < -30.0) {
        eta = -30.0;
    }
    return eta;
}

static double hug_eta(const double *beta, int i, int j, int use_z) {
    int zval = (use_z && hug_include_behav) ? (int)hug_z[(i * hug_t) + j] : 0;
    return hug_eta_z(beta, i, j, zval);
}

static double hug_clampp(double p) {
    if (p < 1e-12) {
        p = 1e-12;
    }
    if (p > 1.0 - 1e-12) {
        p = 1.0 - 1e-12;
    }
    return p;
}

/* Log-verosimiglianza condizionale alla cattura di almeno una volta. */
static double hug_loglik(const double *beta) {
    double ll = 0.0;
    for (int i = 0; i < hug_n; i++) {
        double log_prod_q = 0.0;
        for (int j = 0; j < hug_t; j++) {
            double p  = hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 1)));
            double ps = hug_include_behav ? hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 0))) : p;
            int x = hug_x[(i * hug_t) + j];
            if (x) {
                ll += log(p);
            } else {
                ll += log(1.0 - p);
            }
            log_prod_q += log(1.0 - ps);
        }
        double Pi = 1.0 - exp(log_prod_q);
        if (Pi < 1e-12) {
            Pi = 1e-12;
        }
        ll -= log(Pi);
    }
    return ll;
}

/* Gradiente della log-verosimiglianza condizionale. 0 se manca memoria. */
static int hug_gradient(const double *beta, double *g) {
    for (int r = 0; r < hug_p; r++) {
        g[r] = 0.0;
    }

    double *w  = (double *)malloc((size_t)hug_p * sizeof(double));
    double *ws = (double *)malloc((size_t)hug_p * sizeof(double));
    double *u  = (double *)malloc((size_t)hug_p * sizeof(double));
    if (w == NULL || ws == NULL || u == NULL) {
        free(w);
        free(ws);
        free(u);
        return 0;
    }

    for (int i = 0; i < hug_n; i++) {
        for (int r = 0; r < hug_p; r++) {
            u[r] = 0.0;
        }
        double log_prod_q = 0.0;

        for (int j = 0; j < hug_t; j++) {
            double p  = hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 1)));
            double ps = hug_include_behav ? hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 0))) : p;
            int x = hug_x[(i * hug_t) + j];

            hug_design(i, j, 1, w);
            double res = (double)x - p;
            for (int r = 0; r < hug_p; r++) {
                g[r] += res * w[r];
            }

            hug_design(i, j, 0, ws);
            for (int r = 0; r < hug_p; r++) {
                u[r] += ps * ws[r];
            }
            log_prod_q += log(1.0 - ps);
        }

        double Qi = exp(log_prod_q);
        double Pi = 1.0 - Qi;
        if (Pi < 1e-12) {
            Pi = 1e-12;
        }
        double fct = Qi / Pi;
        for (int r = 0; r < hug_p; r++) {
            g[r] -= fct * u[r];
        }
    }

    free(w);
    free(ws);
    free(u);
    return 1;
}

/* Hessiana della log-verosimiglianza condizionale, definita negativa all'ottimo.
 * 0 se manca memoria. */
static int hug_hessian(const double *beta, double *H) {
    int p_ = hug_p;
    for (int r = 0; r < p_ * p_; r++) {
        H[r] = 0.0;
    }

    double *w  = (double *)malloc((size_t)p_ * sizeof(double));
    double *ws = (double *)malloc((size_t)p_ * sizeof(double));
    double *u  = (double *)malloc((size_t)p_ * sizeof(double));
    double *A  = (double *)malloc((size_t)p_ * (size_t)p_ * sizeof(double)); /* termine del denominatore, per nodo */
    if (w == NULL || ws == NULL || u == NULL || A == NULL) {
        free(w);
        free(ws);
        free(u);
        free(A);
        return 0;
    }

    for (int i = 0; i < hug_n; i++) {
        for (int r = 0; r < p_; r++) {
            u[r] = 0.0;
        }
        for (int r = 0; r < p_ * p_; r++) {
            A[r] = 0.0;
        }
        double log_prod_q = 0.0;

        for (int j = 0; j < hug_t; j++) {
            double p  = hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 1)));
            double ps = hug_include_behav ? hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 0))) : p;

            hug_design(i, j, 1, w);
            double vw = p * (1.0 - p);
            for (int r = 0; r < p_; r++) {
                if (w[r] == 0.0) {
                    continue;
                }
                for (int s = 0; s < p_; s++) {
                    H[(r * p_) + s] -= vw * w[r] * w[s];
                }
            }

            hug_design(i, j, 0, ws);
            double vs = ps * (1.0 - ps);
            for (int r = 0; r < p_; r++) {
                u[r] += ps * ws[r];
                if (ws[r] == 0.0) {
                    continue;
                }
                for (int s = 0; s < p_; s++) {
                    A[(r * p_) + s] += vs * ws[r] * ws[s];
                }
            }
            log_prod_q += log(1.0 - ps);
        }

        double Qi = exp(log_prod_q);
        double Pi = 1.0 - Qi;
        if (Pi < 1e-12) {
            Pi = 1e-12;
        }
        double c1 = Qi / (Pi * Pi);
        double c2 = Qi / Pi;
        for (int r = 0; r < p_; r++) {
            for (int s = 0; s < p_; s++) {
                H[(r * p_) + s] += (c1 * u[r] * u[s]) - (c2 * A[(r * p_) + s]);
            }
        }
    }

    /* Simmetrizza, eliminando il rumore di arrotondamento. */
    for (int r = 0; r < p_; r++) {
        for (int s = r + 1; s < p_; s++) {
            double m = 0.5 * (H[(r * p_) + s] + H[(s * p_) + r]);
            H[(r * p_) + s] = m;
            H[(s * p_) + r] = m;
        }
    }

    free(w);
    free(ws);
    free(u);
    free(A);
    return 1;
}

/* Newton-Raphson. Ritorna 1 se i parametri sono finiti; *converged dice se
 * la convergenza e' stata raggiunta. */
static int hug_fit(double *beta, double *cov_beta, double *ll_out, int *converged) {
    int p_ = hug_p;
    double *g  = (double *)malloc((size_t)p_ * sizeof(double));
    double *H  = (double *)malloc((size_t)p_ * (size_t)p_ * sizeof(double));
    double *Hi = (double *)malloc((size_t)p_ * (size_t)p_ * sizeof(double));
    double *bn = (double *)malloc((size_t)p_ * sizeof(double));
    if (g == NULL || H == NULL || Hi == NULL || bn == NULL) {
        free(g);
        free(H);
        free(Hi);
        free(bn);
        if (converged != NULL) {
            *converged = 0;
        }
        return 0;
    }

    for (int r = 0; r < p_; r++) {
        beta[r] = 0.0;
    }
    if (converged != NULL) {
        *converged = 0;
    }

    double ll_old = hug_loglik(beta);
    int done = 0;

    for (int iter = 0; iter < 200 && !done; iter++) {
        /* memoria esaurita: *converged resta 0 */
        if (!hug_gradient(beta, g)) break;

        double gmax = 0.0;
        for (int r = 0; r < p_; r++) {
            if (fabs(g[r]) > gmax) {
                gmax = fabs(g[r]);
            }
        }
        if (gmax < 1e-7) {
            done = 1;
            if (converged != NULL) {
                *converged = 1;
            }
            break;
        }

        /* memoria esaurita: *converged resta 0 */
        if (!hug_hessian(beta, H)) break;
        /* Hessiana singolare: modello non identificabile. */
        if (!hug_invert(H, p_, Hi)) break;

        double step = 1.0;
        int accepted = 0;
        double ll_new = ll_old;
        for (int bt = 0; bt < 40; bt++) {
            for (int r = 0; r < p_; r++) {
                double dstep = 0.0;
                for (int s = 0; s < p_; s++) {
                    dstep += Hi[(r * p_) + s] * g[s];
                }
                bn[r] = beta[r] - (step * dstep);   /* passo di Newton (Hessiana definita negativa) */
            }
            ll_new = hug_loglik(bn);
            if (isfinite(ll_new) && ll_new > ll_old) {
                accepted = 1;
                break;
            }
            step *= 0.5;
        }

        if (!accepted) {
            /* Nessun passo migliora: convergenza solo se il gradiente e' gia' piccolo. */
            if (converged != NULL) {
                *converged = (gmax < 1e-4) ? 1 : 0;
            }
            break;
        }

        for (int r = 0; r < p_; r++) {
            beta[r] = bn[r];
        }

        if (fabs(ll_new - ll_old) < 1e-10 * (1.0 + fabs(ll_old))) {
            if (hug_gradient(beta, g)) {
                gmax = 0.0;
                for (int r = 0; r < p_; r++) {
                    if (fabs(g[r]) > gmax) {
                        gmax = fabs(g[r]);
                    }
                }
                if (gmax < 1e-4 && converged != NULL) {
                    *converged = 1;
                }
            }
            done = 1;
        }
        ll_old = ll_new;
    }

    int finite_ok = 1;
    for (int r = 0; r < p_; r++) {
        if (!isfinite(beta[r])) {
            finite_ok = 0;
        }
    }
    if (ll_out != NULL) {
        *ll_out = hug_loglik(beta);
    }

    /* Covarianza delle stime: inversa dell'informazione osservata, cioe'
     * dell'Hessiana cambiata di segno. */
    if (cov_beta != NULL) {
        int hess_ok = hug_hessian(beta, H);
        for (int r = 0; hess_ok && r < p_ * p_; r++) {
            H[r] = -H[r];
        }
        if (!hess_ok || !hug_invert(H, p_, cov_beta)) {
            for (int r = 0; r < p_ * p_; r++) {
                cov_beta[r] = 0.0;
            }
        }
    }

    free(g);
    free(H);
    free(Hi);
    free(bn);
    return finite_ok;
}

/* Stima di N come somma degli inversi delle probabilita' di essere catturati almeno una volta; varianza a parametri noti.
 * Derivata della stima rispetto ai parametri, per aggiungere l'incertezza della loro stima. */
static int hug_nhat(const double *beta, double *Nhat_out, double *s2_out, double *dN, double *minP_out, double *maxeta_out) {
    double Nhat = 0.0;
    double s2 = 0.0;
    double minP = 1.0;
    double maxeta = 0.0;   /* massimo modulo del predittore: ~30 = separazione completa */

    if (dN != NULL) {
        for (int r = 0; r < hug_p; r++) {
            dN[r] = 0.0;
        }
    }

    double *ws = (double *)malloc((size_t)hug_p * sizeof(double));
    double *u  = (double *)malloc((size_t)hug_p * sizeof(double));
    if (ws == NULL || u == NULL) {
        free(ws);
        free(u);
        return 0;
    }

    for (int i = 0; i < hug_n; i++) {
        for (int r = 0; r < hug_p; r++) {
            u[r] = 0.0;
        }
        double log_prod_q = 0.0;
        for (int j = 0; j < hug_t; j++) {
            /* Le occasioni congelate non entrano nella diagnostica di separazione. */
            if (!hug_frozen(j)) {
                double eta_ij = hug_eta(beta, i, j, 0);
                double ae = fabs(eta_ij);
                if (ae > maxeta) {
                    maxeta = ae;
                }
            }
            double ps = hug_clampp(hug_sigmoid(hug_eta(beta, i, j, 0)));
            hug_design(i, j, 0, ws);
            for (int r = 0; r < hug_p; r++) {
                u[r] += ps * ws[r];
            }
            log_prod_q += log(1.0 - ps);
        }
        double Qi = exp(log_prod_q);
        double Pi = 1.0 - Qi;
        if (Pi < 1e-12) {
            Pi = 1e-12;
        }
        if (Pi < minP) {
            minP = Pi;
        }

        Nhat += 1.0 / Pi;
        s2 += Qi / (Pi * Pi); /* contributo alla varianza a parametri noti */
        if (dN != NULL) {
            double cf = Qi / (Pi * Pi);
            for (int r = 0; r < hug_p; r++) {
                dN[r] -= cf * u[r];
            }
        }
    }

    free(ws);
    free(u);

    if (Nhat_out != NULL) {
        *Nhat_out = Nhat;
    }
    if (s2_out != NULL) {
        *s2_out = s2;
    }
    if (minP_out != NULL) {
        *minP_out = minP;
    }
    if (maxeta_out != NULL) {
        *maxeta_out = maxeta;
    }
    return isfinite(Nhat);
}

/* Generatore xorshift a 64 bit. */
static uint64_t hug_rng(void) {
    uint64_t x = hug_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    hug_rng_state = x;
    return x;
}

/* Azzera lo stato globale (su ogni uscita, niente puntatori pendenti). */
static void hug_reset_stato(void) {
    hug_n = 0;
    hug_t = 0;
    hug_x = NULL;
    hug_z = NULL;
    hug_cov = NULL;
    hug_include_time = 0;
    hug_include_behav = 0;
    hug_n_cov = 0;
    hug_p = 0;
    hug_ib = 0;
    hug_ic = 0;
    hug_occ_par = NULL;
    hug_occ_empty = NULL;
}

/* ---- Gestione dei buffer di lavoro ---- */

static void hug_buffers_free(HugBuffers_t *b) {
    free(b->occ_empty);
    free(b->occ_par);
    free(b->xh);
    free(b->zh);
    free(b->cat_of);
    free(b->cat_cnt);
    free(b->cat_par);
    free(b->cov_use);
    memset(b, 0, sizeof(*b));
}

/* cov_use dimensionato sul numero di categorie, limite superiore delle indicatrici. */
static int hug_buffers_alloc(HugBuffers_t *b, int n, int t, int ncat) {
    memset(b, 0, sizeof(*b));
    b->occ_empty = (unsigned char *)calloc((size_t)t, sizeof(unsigned char));
    b->occ_par = (int *)malloc((size_t)t * sizeof(int));
    b->xh = (unsigned char *)malloc((size_t)n * (size_t)t);
    b->zh = (unsigned char *)malloc((size_t)n * (size_t)t);
    b->cat_of = (int *)malloc((size_t)n * sizeof(int));
    b->cat_cnt = (int *)calloc((size_t)ncat, sizeof(int));
    b->cat_par = (int *)malloc((size_t)ncat * sizeof(int));
    b->cov_use = (double *)calloc((size_t)n * (size_t)ncat, sizeof(double));

    if (b->occ_empty == NULL || b->occ_par == NULL || b->xh == NULL || b->zh == NULL
        || b->cat_of == NULL || b->cat_cnt == NULL || b->cat_par == NULL || b->cov_use == NULL) {
        hug_buffers_free(b);
        return 0;
    }
    return 1;
}

/* ---- Fase 1: censimento delle occasioni ----
 * Riempie occ_empty e stampa le note. Ritorna le occasioni attive, -1 se
 * manca memoria. */
static int hug_analizza_occasioni(const EstimatorData_t *d, unsigned char *occ_empty) {
    int n = d->n_nodes;
    int t = d->t;

    int *occ_tot = (int *)calloc((size_t)t, sizeof(int));
    if (occ_tot == NULL) return -1;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < t; j++) {
            if (d->seen[i][j]) {
                occ_tot[j]++;
            }
        }
    }

    int n_active = 0;
    int n_full = 0;
    for (int j = 0; j < t; j++) {
        if (occ_tot[j] > 0) {
            n_active++;
            occ_empty[j] = 0;
        } else {
            occ_empty[j] = 1;
        }
        if (occ_tot[j] == n) {
            n_full++;
        }
    }
    free(occ_tot);

    fprintf(stdout, "Occasioni di cattura: t = %d (attive: %d)\n", t, n_active);
    if (n_active < t) {
        fprintf(stdout, "NOTA: %d occasione/i senza catture. Restano nel campione (sotto M_0 e M_b\n", t - n_active);
        fprintf(stdout, "      sono informative); nei modelli con effetti tempo il loro effetto ha\n");
        fprintf(stdout, "      stima di massima verosimiglianza -infinito e viene posto al limite p=0.\n");
    }
    if (n_full > 0) {
        fprintf(stdout, "NOTA: %d occasione/i con TUTTI i nodi catturati: separazione completa,\n", n_full);
        fprintf(stdout, "      il relativo coefficiente e' limitato numericamente (eta in [-30,30]).\n");
    }
    return n_active;
}

/* ---- Fase 2: storie di cattura x e indicatori di cattura pregressa z ---- */
static void hug_costruisci_storie(const EstimatorData_t *d, unsigned char *xh, unsigned char *zh) {
    int t = d->t;
    for (int i = 0; i < d->n_nodes; i++) {
        int prev = 0;
        for (int j = 0; j < t; j++) {
            unsigned char v = d->seen[i][j] ? 1 : 0;
            xh[(i * t) + j] = v;
            zh[(i * t) + j] = (unsigned char)prev;   /* 1 se catturato prima di questa occasione */
            if (v) {
                prev = 1;
            }
        }
    }
}

/* Nodi con almeno due catture: senza ricatture nessun modello e' identificabile. */
static int hug_conta_ricatture(const unsigned char *xh, int n, int t) {
    int n_recap = 0;
    for (int i = 0; i < n; i++) {
        int s = 0;
        for (int j = 0; j < t; j++) {
            s += xh[(i * t) + j];
        }
        if (s >= 2) {
            n_recap++;
        }
    }
    return n_recap;
}

/* ---- Fase 3: codifica della covariata "sottorete" ----
 * Categorie: sottoreti + "nessuna sottorete"; riferimento = la piu' numerosa.
 * Categorie piccole o oltre il limite confluiscono nel riferimento.
 * Ritorna il numero di indicatrici; cat_ref_out = -1 se covariata disattivata. */
static int hug_codifica_sottoreti(const EstimatorData_t *d, HugBuffers_t *b, int ncat, int *cat_ref_out) {
    int n = d->n_nodes;

    for (int cc = 0; cc < ncat; cc++) {
        b->cat_par[cc] = -1;
    }
    for (int i = 0; i < n; i++) {
        int sn = d->subnet_idx[i];
        if (sn < 0 || sn >= d->n_subnets) {
            sn = d->n_subnets;                     /* pseudo-categoria */
        }
        b->cat_of[i] = sn;
        b->cat_cnt[sn]++;
    }

    int cat_ref = -1;
    int ncov = 0;

    if (HUG_USA_COVARIATE) {
        /* Riferimento = categoria piu' numerosa (scelta piu' stabile). */
        for (int cc = 0; cc < ncat; cc++) {
            if (b->cat_cnt[cc] > 0 && (cat_ref < 0 || b->cat_cnt[cc] > b->cat_cnt[cat_ref])) {
                cat_ref = cc;
            }
        }

        /* Indicatrici alle altre categorie ammissibili, per numerosita' decrescente. */
        int max_dummy = HUG_MAX_CATEGORIE - 1;
        while (ncov < max_dummy) {
            int best_cc = -1;
            for (int cc = 0; cc < ncat; cc++) {
                if (cc == cat_ref || b->cat_par[cc] >= 0) continue;
                if (b->cat_cnt[cc] < HUG_MIN_NODI_CATEGORIA) continue;
                if (best_cc < 0 || b->cat_cnt[cc] > b->cat_cnt[best_cc]) {
                    best_cc = cc;
                }
            }
            if (best_cc < 0) break;
            b->cat_par[best_cc] = ncov;
            ncov++;
        }

        for (int i = 0; i < n; i++) {
            int k = b->cat_par[b->cat_of[i]];
            if (k >= 0) {
                b->cov_use[(i * ncov) + k] = 1.0;
            }
        }
    }

    *cat_ref_out = cat_ref;
    return ncov;
}

static void hug_stampa_codifica(const EstimatorData_t *d, const HugBuffers_t *b, int ncat, int cat_ref, int ncov) {
    fprintf(stdout, "\n--- Covariata individuale: appartenenza alla sottorete ---\n");

    if (!HUG_USA_COVARIATE) {
        fprintf(stdout, "Disattivata (-DHUG_USA_COVARIATE=0): i modelli 4-6 non vengono stimati.\n");
        return;
    }
    if (ncov == 0) {
        fprintf(stdout, "Una sola categoria ammissibile: nessuna indicatrice da costruire,\n");
        fprintf(stdout, "quindi nessuna eterogeneita' individuale modellabile.\n");
        return;
    }

    fprintf(stdout, "%-28s %8s   %s\n", "Categoria", "Nodi", "Codifica");
    for (int cc = 0; cc < ncat; cc++) {
        if (b->cat_cnt[cc] == 0) continue;
        if (cc == cat_ref) {
            fprintf(stdout, "%-28s %8d   riferimento (assorbita da b0)\n", hug_nome_cat(d, cc), b->cat_cnt[cc]);
        } else if (b->cat_par[cc] >= 0) {
            fprintf(stdout, "%-28s %8d   indicatrice c%d\n", hug_nome_cat(d, cc), b->cat_cnt[cc], b->cat_par[cc] + 1);
        } else {
            fprintf(stdout, "%-28s %8d   accorpata al riferimento (< %d nodi o oltre il limite)\n", hug_nome_cat(d, cc), b->cat_cnt[cc], HUG_MIN_NODI_CATEGORIA);
        }
    }
    fprintf(stdout, "Indicatrici costruite: %d (categorie utilizzate: %d su %d con nodi)\n", ncov, ncov + 1, ncat);
}

/* ---- Fase 5: stima dei Modelli 0-6 e scelta del modello con AIC minimo.
 * Ritorna l'indice vincente o -1; in caso di successo parametri e covarianza passano al chiamante. ---- */
static int hug_stima_candidati(int n, int ncov, HugFit_t *fits, double **beta_out, double **covb_out, int *p_out) {
    int best = -1;
    *beta_out = NULL;
    *covb_out = NULL;
    *p_out = 0;

    for (int mi = 0; mi < 7; mi++) {
        fits[mi].p = 0;
        fits[mi].ok = 0;
        fits[mi].ll = 0.0;
        fits[mi].aic = 0.0;
        fits[mi].Nhat = 0.0;
    }

    fprintf(stdout, "\n--- Modelli 0-6 di Huggins (1991, par. 2) ---\n");
    fprintf(stdout, "%-22s %4s %12s %12s %12s\n", "Modello", "s", "logL", "AIC", "N_hat");

    for (int mi = 0; mi < 7; mi++) {
        /* I modelli con covariate richiedono indicatrici ammissibili. */
        if (HUG_MODELLI[mi].cov && ncov == 0) {
            fprintf(stdout, "%-22s %4s %12s %12s %12s   [nessuna covariata ammissibile]\n",
                    HUG_MODELLI[mi].name, "-", "-", "-", "-");
            continue;
        }

        hug_include_time = HUG_MODELLI[mi].time;
        hug_include_behav = HUG_MODELLI[mi].behav;
        hug_n_cov = HUG_MODELLI[mi].cov ? ncov : 0;
        hug_setup_layout();

        /* Tanti parametri quanti nodi: verosimiglianza satura, modello non identificabile. */
        if (hug_p >= n) {
            fprintf(stdout, "%-22s %4d %12s %12s %12s   [s >= S_obs: non identificabile]\n", HUG_MODELLI[mi].name, hug_p, "-", "-", "-");
            continue;
        }

        double *bta = (double *)malloc((size_t)hug_p * sizeof(double));
        double *cvb = (double *)malloc((size_t)hug_p * (size_t)hug_p * sizeof(double));
        if (bta == NULL || cvb == NULL) {
            free(bta);
            free(cvb);
            continue;
        }

        double ll = 0.0;
        int conv = 0;
        int ok = hug_fit(bta, cvb, &ll, &conv);
        double Nh = 0.0;
        double s2 = 0.0;
        double minP = 1.0;
        double maxeta = 0.0;
        int nok = ok ? hug_nhat(bta, &Nh, &s2, NULL, &minP, &maxeta) : 0;

        /* Escluso se al bordo: probabilita' minima quasi nulla (N diverge) o predittore vicino al limite (separazione completa, logL e AIC dipendono alla soglia). */
        int al_bordo = (maxeta >= 30.0 - 1.0);
        int usabile = ok && conv && nok && (minP >= 1e-4) && !al_bordo;

        fits[mi].p = hug_p;
        fits[mi].ll = ll;
        fits[mi].aic = (-2.0 * ll) + (2.0 * (double)hug_p);
        fits[mi].Nhat = Nh;
        fits[mi].ok = usabile;

        if (usabile) {
            fprintf(stdout, "%-22s %4d %12.3f %12.3f %12.1f\n", HUG_MODELLI[mi].name, hug_p, ll, fits[mi].aic, Nh);
        } else if (al_bordo && ok && conv && nok && minP >= 1e-4) {
            fprintf(stdout, "%-22s %4d %12s %12s %12s   [separazione completa: coeff. al bordo]\n", HUG_MODELLI[mi].name, hug_p, "-", "-", "-");
        } else {
            fprintf(stdout, "%-22s %4d %12s %12s %12s   [non stimabile]\n", HUG_MODELLI[mi].name, hug_p, "-", "-", "-");
        }

        if (usabile && (best < 0 || fits[mi].aic < fits[best].aic)) {
            best = mi;
            free(*beta_out);
            free(*covb_out);
            *beta_out = bta;
            *covb_out = cvb;
            *p_out = hug_p;
            bta = NULL;
            cvb = NULL;
        }
        free(bta);
        free(cvb);
    }

    return best;
}

/* ---- Fase 5b: test del rapporto di verosimiglianza fra modelli annidati ----
 * La statistica e' confrontata con una chi-quadro con tanti gradi di liberta' quanti i parametri in piu'. Solo informativo: l'esito dipende dall'ordine dei confronti, e la scelta resta all'AIC. */
static void hug_stampa_lrt(const HugFit_t *fits) {
    fprintf(stdout, "\n--- Test del rapporto di verosimiglianza (Huggins 1991, par. 3) ---\n");
    fprintf(stdout, "%-12s %10s %5s %10s   %s\n", "Confronto", "Lambda", "df", "P", "Esito (5%)");

    for (int li = 0; li < 7; li++) {
        int m0 = HUG_CONFRONTI[li].nullo;
        int m1 = HUG_CONFRONTI[li].alt;

        if (!hug_annidato(m0, m1)) {
            /* Solo se HUG_MODELLI e HUG_CONFRONTI sono incoerenti. */
            fprintf(stdout, "%-2d vs %-7d %10s %5s %10s   modelli non annidati\n", m0, m1, "-", "-", "-");
            continue;
        }
        if (!fits[m0].ok || !fits[m1].ok) {
            fprintf(stdout, "%-2d vs %-7d %10s %5s %10s   non disponibile\n", m0, m1, "-", "-", "-");
            continue;
        }

        int df = fits[m1].p - fits[m0].p;
        double lam = 2.0 * (fits[m1].ll - fits[m0].ll);

        if (df <= 0) {
            fprintf(stdout, "%-2d vs %-7d %10s %5s %10s   df <= 0: confronto non informativo\n", m0, m1, "-", "-", "-");
            continue;
        }
        if (lam < 0.0) {
            /* Solo per convergenza imperfetta del modello piu' ampio. */
            fprintf(stdout, "%-2d vs %-7d %10.3f %5d %10s   Lambda < 0: fit non affidabile\n", m0, m1, lam, df, "-");
            continue;
        }

        double pval = chi2_sf(lam, df);
        fprintf(stdout, "%-2d vs %-7d %10.3f %5d %10.4f   %s\n", m0, m1, lam, df, pval, (pval < 0.05) ? "modello nullo rifiutato" : "modello nullo non rifiutato");
    }
}

/* ---- Fase 7: bootstrap condizionale ----
 * Nodi catturati e covariate restano fissi. Le storie si ri-simulano con i parametri stimati, aggiornando la cattura pregressa e scartando le storie vuote, e si rifa' il fit.
 * Poiche' n e' gia' un limite inferiore, si elimina solo la coda superiore. Ritorna 1 se calcolato.
 */
static int hug_bootstrap(const double *beta, int n, int t, double *boot_hi, double *boot_se, int *nrep_out) {
    const int B = HUG_BOOT_REPLICHE;
    int nrep = 0;
    int ok = 0;

    unsigned char *bx = (unsigned char *)malloc((size_t)n * (size_t)t);
    unsigned char *bz = (unsigned char *)malloc((size_t)n * (size_t)t);
    double *bb = (double *)malloc((size_t)hug_p * sizeof(double));
    double *reps = (double *)malloc((size_t)B * sizeof(double));

    /* Probabilita' di cattura con e senza cattura pregressa, calcolate prima di sostituire le storie osservate. */
    double *p0 = (double *)malloc((size_t)n * (size_t)t * sizeof(double)); /* senza cattura pregressa */
    double *p1 = (double *)malloc((size_t)n * (size_t)t * sizeof(double)); /* con cattura pregressa */

    unsigned char *ox = hug_x;
    unsigned char *oz = hug_z;

    if (bx != NULL && bz != NULL && bb != NULL && reps != NULL && p0 != NULL && p1 != NULL) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < t; j++) {
                p0[(i * t) + j] = hug_clampp(hug_sigmoid(hug_eta_z(beta, i, j, 0)));
                if (hug_include_behav) {
                    p1[(i * t) + j] = hug_clampp(hug_sigmoid(hug_eta_z(beta, i, j, 1)));
                } else {
                    p1[(i * t) + j] = p0[(i * t) + j];
                }
            }
        }

        for (int b = 0; b < B; b++) {
            for (int i = 0; i < n; i++) {
                int any = 0;
                int guard = 0;
                do {
                    int prev = 0;
                    any = 0;
                    for (int j = 0; j < t; j++) {
                        double pij = prev ? p1[(i * t) + j] : p0[(i * t) + j];
                        double u = (double)(hug_rng() >> 11) / 9007199254740992.0;
                        unsigned char v = (u < pij) ? 1 : 0;
                        bx[(i * t) + j] = v;
                        bz[(i * t) + j] = (unsigned char)prev;
                        if (v) {
                            prev = 1;
                            any = 1;
                        }
                    }
                    guard++;
                } while (!any && guard < 1000);

                if (!any) {
                    /* P_i praticamente nulla: si tiene la storia originale. */
                    memcpy(bx + ((size_t)i * (size_t)t), ox + ((size_t)i * (size_t)t), (size_t)t);
                    memcpy(bz + ((size_t)i * (size_t)t), oz + ((size_t)i * (size_t)t), (size_t)t);
                }
            }

            hug_x = bx;
            hug_z = bz;

            double llb = 0.0;
            int convb = 0;
            if (hug_fit(bb, NULL, &llb, &convb) && convb) {
                double Nb = 0.0;
                double s2b = 0.0;
                double mpb = 1.0;
                if (hug_nhat(bb, &Nb, &s2b, NULL, &mpb, NULL) && mpb >= 1e-4) {
                    reps[nrep] = Nb;
                    nrep++;
                }
            }
        }
        hug_x = ox;
        hug_z = oz;

        if (nrep >= 2) {
            /* Ordinamento per inserzione (nrep <= B). */
            for (int a = 1; a < nrep; a++) {
                double v = reps[a];
                int k = a - 1;
                while (k >= 0 && reps[k] > v) {
                    reps[k + 1] = reps[k];
                    k--;
                }
                reps[k + 1] = v;
            }
            /* 95-esimo percentile: si elimina solo la coda superiore. */
            *boot_hi = reps[(int)(0.95 * (double)(nrep - 1))];

            double mu = 0.0;
            for (int k = 0; k < nrep; k++) {
                mu += reps[k];
            }
            mu /= (double)nrep;

            double ss = 0.0;
            for (int k = 0; k < nrep; k++) {
                ss += (reps[k] - mu) * (reps[k] - mu);
            }
            *boot_se = sqrt(ss / (double)(nrep - 1));
            ok = 1;
        }
    }

    free(bx);
    free(bz);
    free(bb);
    free(reps);
    free(p0);
    free(p1);

    *nrep_out = nrep;
    return ok;
}

/* ---- Fase 8: descrizione testuale del modello selezionato ---- */
static void hug_descrivi_modello(char *buf, size_t bufsz) {
    /* Solo il blocco delle covariate ha un campo variabile; snprintf tronca da solo. */
    char cov_desc[64] = "";
    if (hug_n_cov > 0) {
        snprintf(cov_desc, sizeof(cov_desc), " + b'c_i (sottorete, %d indicatrici)", hug_n_cov);
    }
    snprintf(buf, bufsz, "logit(p_ij) = b0%s%s%s", hug_include_time  ? " + beta_j (occasione)"       : "", hug_include_behav ? " + bb*z_ij (comportamento)" : "", cov_desc);
}

/* Famiglia del modello: senza covariate e' omogeneo tra individui. */
static void hug_descrivi_famiglia(char *buf, size_t bufsz) {
    char suf[4];
    int nsuf = 0;
    if (hug_include_time) {
        suf[nsuf] = 't';
        nsuf++;
    }
    if (hug_include_behav) {
        suf[nsuf] = 'b';
        nsuf++;
    }
    if (hug_n_cov > 0) {
        suf[nsuf] = 'h';
        nsuf++;
    }
    suf[nsuf] = '\0';

    snprintf(buf, bufsz, "M_%s (%s)", nsuf ? suf : "0", hug_n_cov > 0 ? "eterogeneo tra individui" : "omogeneo tra individui");
}

/* Coefficienti con errori standard dalla diagonale della covarianza stimata. */
static void hug_stampa_coefficienti(const EstimatorData_t *d, const double *beta, const double *covb, const HugBuffers_t *b, int ncat, int cat_ref) {
    fprintf(stdout, "\n--- Coefficienti stimati ---\n");
    fprintf(stdout, "%-34s %12s %10s\n", "Parametro", "Stima", "Err.Std");

    for (int r = 0; r < hug_p; r++) {
        char nome[64];

        if (r == 0) {
            snprintf(nome, sizeof(nome), "b0 (intercetta)");
        } else if (hug_include_behav && r == hug_ib) {
            snprintf(nome, sizeof(nome), "bb (comportamento, z_ij)");
        } else if (r >= hug_ic && r < hug_ic + hug_n_cov) {
            int k = r - hug_ic;
            int cc = -1;
            for (int c2 = 0; c2 < ncat; c2++) {
                if (b->cat_par[c2] == k) {
                    cc = c2;
                }
            }
            snprintf(nome, sizeof(nome), "c%d  %.24s", k + 1, hug_nome_cat(d, cc));
        } else {
            int occ = -1;
            for (int j = 0; j < hug_t; j++) {
                if (hug_occ_par[j] == r) {
                    occ = j;
                }
            }
            snprintf(nome, sizeof(nome), "beta_%d (occasione %d)", occ + 1, occ + 1);
        }

        double vr = covb[(r * hug_p) + r];
        fprintf(stdout, "%-34s %12.4f %10.4f\n", nome, beta[r], vr > 0.0 ? sqrt(vr) : 0.0);
    }

    if (hug_n_cov > 0) {
        fprintf(stdout, "Categoria di riferimento (b0): %s\n", hug_nome_cat(d, cat_ref));
    }
}

void node_huggins_estimators_print_stats(const EstimatorData_t *d) {
    int n = d->n_nodes;
    int t = d->t;
    if (n <= 0 || t <= 0) return;

    fprintf(stdout, "\n=============== STIMATORE DI HUGGINS (verosimiglianza condizionale) ===============\n");
    if (t < 2) {
        fprintf(stdout, "ATTENZIONE: t=%d < 2. Con una sola occasione il modello di Huggins non e'\n", t);
        fprintf(stdout, "identificabile (nessuna informazione sulla ricattura). Stima non calcolata.\n");
        return;
    }

    const int ncat = d->n_subnets + 1;         /* +1: "nessuna sottorete" */
    HugBuffers_t buf;
    if (!hug_buffers_alloc(&buf, n, t, ncat)) {
        fprintf(stdout, "Memoria insufficiente per la stima di Huggins.\n");
        return;
    }

    /* ---- 1. Censimento occasioni: nessuna eliminata, le vuote vengono congelate ---- */
    int n_active = hug_analizza_occasioni(d, buf.occ_empty);
    if (n_active < 0) {
        fprintf(stdout, "Memoria insufficiente per la stima di Huggins.\n");
        hug_buffers_free(&buf);
        return;
    }
    if (n_active < 2) {
        fprintf(stdout, "Occasioni attive < 2: modello non identificabile. Stima non calcolata.\n");
        hug_buffers_free(&buf);
        return;
    }

    /* ---- 2-3. Storie di cattura e codifica della covariata ---- */
    hug_costruisci_storie(d, buf.xh, buf.zh);

    int cat_ref = -1;
    int ncov = hug_codifica_sottoreti(d, &buf, ncat, &cat_ref);
    hug_stampa_codifica(d, &buf, ncat, cat_ref, ncov);

    int n_recap = hug_conta_ricatture(buf.xh, n, t);
    fprintf(stdout, "Nodi con almeno una ricattura: %d su %d (%.1f%%)\n", n_recap, n, 100.0 * (double)n_recap / (double)n);
    if (n_recap == 0) {
        fprintf(stdout, "Nessuna ricattura: N non e' identificabile (qualsiasi N >= %d e'\n", n);
        fprintf(stdout, "compatibile con i dati). Stima non calcolata.\n");
        hug_buffers_free(&buf);
        return;
    }

    /* ---- 4. Contesto globale ---- */
    hug_n = n;
    hug_t = t;
    hug_x = buf.xh;
    hug_z = buf.zh;
    hug_cov = buf.cov_use;
    hug_occ_empty = buf.occ_empty;
    hug_occ_par = buf.occ_par;

    /* ---- 5. Selezione del modello per AIC, poi test annidati ---- */
    HugFit_t fits[7];
    double *beta = NULL;
    double *covb = NULL;
    int best_p = 0;
    int best = hug_stima_candidati(n, ncov, fits, &beta, &covb, &best_p);

    if (best < 0) {
        fprintf(stdout, "\nNessun modello e' arrivato a convergenza: stima di Huggins NON calcolata.\n");
        fprintf(stdout, "(Tipicamente: separazione quasi-completa o pochissime ricatture.)\n");
        free(beta);
        free(covb);
        hug_buffers_free(&buf);
        hug_reset_stato();
        return;
    }

    hug_stampa_lrt(fits);

    /* ---- 6. Ripristino del modello vincente e stima finale ---- */
    hug_include_time  = HUG_MODELLI[best].time;
    hug_include_behav = HUG_MODELLI[best].behav;
    hug_n_cov = HUG_MODELLI[best].cov ? ncov : 0;
    hug_setup_layout();

    /* Il numero di parametri deve coincidere con quello del fit salvato. */
    if (hug_p != best_p) {
        fprintf(stdout, "Errore interno: layout dei parametri incoerente. Stima annullata.\n");
        free(beta);
        free(covb);
        hug_buffers_free(&buf);
        hug_reset_stato();
        return;
    }

    double *dN = (double *)calloc((size_t)hug_p, sizeof(double));
    if (dN == NULL) {
        fprintf(stdout, "Memoria insufficiente per la stima di Huggins.\n");
        free(beta);
        free(covb);
        hug_buffers_free(&buf);
        hug_reset_stato();
        return;
    }

    double S = (double)n;
    double Nhat = 0.0;
    double var_ht = 0.0;
    if (!hug_nhat(beta, &Nhat, &var_ht, dN, NULL, NULL)) {
        fprintf(stdout, "Memoria insufficiente per la stima di Huggins.\n");
        free(dN);
        free(beta);
        free(covb);
        hug_buffers_free(&buf);
        hug_reset_stato();
        return;
    }

    double var_beta = 0.0;
    for (int r = 0; r < hug_p; r++) {
        for (int c = 0; c < hug_p; c++) {
            var_beta += dN[r] * covb[(r * hug_p) + c] * dN[c];
        }
    }
    free(dN);

    /* In un massimo la covarianza e' definita positiva: un valore negativo indica
     * un fit fermo in un punto di sella o al bordo (segnalato). */
    int var_beta_neg = 0;
    if (var_beta < 0.0) {
        var_beta_neg = 1;
        var_beta = 0.0;
    }

    double var_tot = var_ht + var_beta;
    double se = sqrt(var_tot > 0.0 ? var_tot : 0.0);
    double diff = Nhat - S;

    double ci_lo_cl = Nhat - (1.96 * se);
    double ci_hi_cl = Nhat + (1.96 * se);
    if (ci_lo_cl < S) {
        ci_lo_cl = S;                  /* N non puo' essere minore degli osservati */
    }

    /* Intervalli: normale asintotico, pratico (dagli osservati a N piu' due errori standard) e bootstrap condizionale. */

    /* ---- 7. Bootstrap condizionale ---- */
    double boot_hi = 0.0;
    double boot_se = 0.0;
    int nrep = 0;
    int boot_ok = hug_bootstrap(beta, n, t, &boot_hi, &boot_se, &nrep);

    /* ---- 8. Stampa ---- */
    char model_desc[192];
    char fam[64];
    hug_descrivi_modello(model_desc, sizeof(model_desc));
    hug_descrivi_famiglia(fam, sizeof(fam));

    fprintf(stdout, "\n--- Modello selezionato (AIC minimo): %s ---\n", HUG_MODELLI[best].name);
    fprintf(stdout, "%s\n", model_desc);
    fprintf(stdout, "Famiglia: %s\n", fam);
    if (hug_include_time) {
        fprintf(stdout, "Vincolo di identificabilita': beta = 0 sull'ultima occasione attiva\n");
        fprintf(stdout, "(Huggins 1991, Modello 2: beta_t = 0).\n");
    }
    if (hug_n_cov == 0) {
        fprintf(stdout, "AVVISO: nessuna covariata individuale ammissibile, quindi P_i e' uguale per\n");
        fprintf(stdout, "        tutti i nodi: NON c'e' eterogeneita' individuale e N_hat coincide\n");
        fprintf(stdout, "        con lo stimatore classico n/P_hat.\n");
    }
    fprintf(stdout, "Occasioni t = %d, nodi osservati S_obs = %d, parametri s = %d\n", t, n, hug_p);
    fprintf(stdout, "logL condizionale = %.4f, AIC = %.4f\n", fits[best].ll, fits[best].aic);

    hug_stampa_coefficienti(d, beta, covb, &buf, ncat, cat_ref);

    fprintf(stdout, "\n--- Stima della popolazione (pesi 1/P_i) ---\n");
    fprintf(stdout, "Stimatore di Huggins: %.2f nodi totali stimati\n", Nhat);
    fprintf(stdout, " - Osservati direttamente: %d\n", n);
    fprintf(stdout, " - Potenzialmente nascosti: circa %.0f\n", diff > 0.0 ? diff : 0.0);
    fprintf(stdout, "\nVarianza stimata: %.4f (s^2: %.4f + stima di beta: %.4f), Dev.Std = %.2f\n", var_tot, var_ht, var_beta, se);
    if (var_beta_neg) {
        fprintf(stdout, "ATTENZIONE: la componente D' Cov(beta) D e' risultata negativa. Poiche'\n");
        fprintf(stdout, "            Cov(beta) = (-H)^-1 e' definita positiva in un massimo genuino,\n");
        fprintf(stdout, "            il fit si e' probabilmente fermato in un punto di sella o al bordo\n");
        fprintf(stdout, "            dello spazio dei parametri: la varianza e' SOTTOSTIMATA.\n");
    }

    /* Intervallo basato sulla normalita' asintotica della stima. */
    fprintf(stdout, "IC 95%% (normale asintotico, troncato a S_obs): [%.2f, %.2f]\n", ci_lo_cl, ci_hi_cl);
    /* Intervallo pratico, dagli osservati a N piu' due errori standard: in genere vicino al bootstrap. */
    fprintf(stdout, "IC 95%% (Huggins 1989, estremo inferiore = n): [%.2f, %.2f]\n", S, Nhat + (2.0 * se));
    /* Bootstrap condizionale: estremo inferiore pari agli osservati, coda superiore eliminata. */
    if (boot_ok) {
        fprintf(stdout, "IC 95%% (bootstrap condizionale, B=%d, %d repliche valide): [%.2f, %.2f]\n", HUG_BOOT_REPLICHE, nrep, S, boot_hi);
        fprintf(stdout, "Dev.Std bootstrap condizionale: %.2f (asintotica: %.2f)\n", boot_se, se);
    } else {
        fprintf(stdout, "Bootstrap condizionale non calcolabile (%d repliche valide su %d).\n", nrep, HUG_BOOT_REPLICHE);
    }

    if (Nhat > 20.0 * S) {
        fprintf(stdout, "\nATTENZIONE: N_hat/S_obs = %.1f. Rapporto molto alto: l'estrapolazione e'\n", Nhat / S);
        fprintf(stdout, "instabile, tratta la stima come un limite inferiore e non come un valore puntuale.\n");
    }

    /* ---- 9. Pulizia ---- */
    free(beta);
    free(covb);
    hug_buffers_free(&buf);
    hug_reset_stato();
}