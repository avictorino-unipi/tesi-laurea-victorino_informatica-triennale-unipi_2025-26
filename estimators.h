#ifndef ESTIMATORS_H
#define ESTIMATORS_H

#include <stdint.h>

/* Dati di cattura passati agli stimatori */
typedef struct {
    int t;                      /* occasioni di cattura */
    int n_nodes;                /* nodi distinti osservati (S_obs) */
    uint8_t **seen;             /* seen[i][j] = 1 se nodo i visto in j */
    int *subnet_idx;            /* sottorete del nodo i, -1 se nessuna */
    int n_subnets;              /* numero di sottoreti */
    uint8_t *subnet_version;    /* versione IP (4 o 6) */
    char **subnet_name;         /* es. "10.0.0.0/24" */
    char **subnet_range_end;    /* ultimo indirizzo */
    int *subnet_flow_count;     /* flussi osservati */
} EstimatorData_t;

/* Calcolo e stampa degli stimatori */
void node_jackknife_estimators_print_stats(const EstimatorData_t *d);
void node_chao_estimators_print_stats(const EstimatorData_t *d);
void node_huggins_estimators_print_stats(const EstimatorData_t *d);

/* Statistiche per sottorete */
void node_subnet_capture_probability_print_stats(const EstimatorData_t *d);

#endif /* ESTIMATORS_H */