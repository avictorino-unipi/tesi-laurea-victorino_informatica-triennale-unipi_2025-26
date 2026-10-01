#define _GNU_SOURCE
/* ========== Librerie Standard ========== */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
/* ========== Librerie per la gestione dei privilegi ========== */
#include <pwd.h>
#include <sys/capability.h>
#include <sys/prctl.h>
/* ========== Librerie per l'elaborazione dei pacchetti ========== */
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <pcap/pcap.h>

#include "estimators.h" /* Modulo degli stimatori (jackknife, Chao, Huggins) */

static int NUM_CAPTURE = 0;
static char hex[] = "0123456789ABCDEF";

// #define DEFAULT_SNAPLEN 65535
pcap_t *pd;
struct pcap_stat pcapStats;
char **captures = NULL;
pcap_t **pd_array = NULL;

typedef union {
    uint32_t v4;                    /* IPv4, network byte order */
    struct in6_addr v6;             /* IPv6, 16 byte grezzi */
} ip_union_t;

#define NODE_HASH_SIZE 4096         /* Bucket (potenza di 2) */

typedef struct Node {
    char mac[18];                   /* MAC "XX:XX:XX:XX:XX:XX": identità del nodo */
    uint8_t has_ipv4;               /* 1 se osservato un IPv4 */
    uint8_t has_ipv6;               /* 1 se osservato un IPv6 */
    uint32_t ipv4;                  /* primo IPv4 osservato (se has_ipv4) */
    struct in6_addr ipv6;           /* primo IPv6 osservato (se has_ipv6) */
    int subnet_idx;                 /* unica sottorete del nodo, -1 se nessuna */
    uint8_t *seen;                  /* seen[c] = 1 se visto nella cattura c */
    struct Node *next;              /* catena del bucket */
} Node_t;

static Node_t *node_table[NODE_HASH_SIZE];  /* tabella hash dei nodi */
static Node_t **node_list = NULL;           /* nodi in ordine di inserimento */
static int node_list_cap = 0;               /* capacità di node_list */
static int total_nodes = 0;                 /* nodi unici osservati (S_obs) */

#define MAX_SUBNETS 512                         /* sottoreti massime */
#define ARP_PSEUDO_PROTOCOL 254                 /* protocollo fittizio per ARP (RFC 3692, uso sperimentale) */
#define IPV4_SUBNET_PREFIX 24                   /* prefisso sottoreti IPv4 */
#define IPV6_SUBNET_PREFIX 64                   /* prefisso sottoreti IPv6 */
#define SUBNET_STR_LEN (INET6_ADDRSTRLEN + 8)   /* "indirizzo/prefisso" */

/* Ogni nodo appartiene a una sola sottorete, anche se dual-stack, cosi' le
 * popolazioni per sottorete sono disgiunte e sommano a S_obs.
 *   1 -> sottorete IPv4 (altrimenti IPv6)
 *   0 -> sottorete IPv6 (altrimenti IPv4) */
#define NODE_SUBNET_PREFER_IPV4 1

typedef struct Subnet {
    char subnet[SUBNET_STR_LEN];                /* es. "192.168.1.0/24", "fd00::/64" */
    char range_end[INET6_ADDRSTRLEN];           /* ultimo indirizzo */
    uint8_t version_ip;                         /* 4 o 6 */
    int prefix_len;                             /* lunghezza del prefisso */
    int node_count;                             /* nodi attribuiti */
    int flow_count;                             /* flussi con sorgente qui */
} Subnet_t;

static Subnet_t subnet_stats[MAX_SUBNETS];
static int total_subnets = 0;

typedef struct FlowKey {
    uint8_t version_ip;
    ip_union_t src_ip;
    ip_union_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
} FlowKey_t;

/* ---------- Tabella hash dei flussi (chiave: 5-tupla) ---------- */
#define FLOW_HASH_SIZE 65536        /* bucket (potenza di 2) */

typedef struct FlowEntry {
    FlowKey_t key;                  /* 5-tupla */
    uint64_t packets;               /* pacchetti */
    uint64_t bytes;                 /* byte (lunghezza originale) */
    struct timeval first_ts;        /* primo pacchetto */
    struct timeval last_ts;         /* ultimo pacchetto */
    int subnet_idx;                 /* sottorete della sorgente, -1 se ignota */
    struct FlowEntry *next;         /* catena del bucket */
} FlowEntry_t;

static FlowEntry_t *flow_table[FLOW_HASH_SIZE];  /* tabella hash dei flussi */
static int total_flows = 0;                      /* flussi unici (liberati solo a fine esecuzione) */


/* Prototipi di funzione */
Node_t *node_lookup(const char *mac);
void save_node(int capture_idx, const char *mac, uint8_t version_ip, ip_union_t *ip);
int is_node_valid(uint8_t version_ip, const ip_union_t *ip);

void ipv4_to_subnet(uint32_t ip_net, int prefix_len, char *out, size_t out_len);
void ipv6_to_subnet(const struct in6_addr *addr_in, int prefix_len, char *out, size_t out_len);
void ipv4_last_addr_from_network(const char *network_str, int prefix_len, char *out, size_t out_len);
void ipv6_last_addr_from_network(const char *network_str, int prefix_len, char *out, size_t out_len);
int create_subnet(uint8_t version_ip, const char *subnet_str, int prefix_len);
int node_compute_subnet(const Node_t *n);
void node_update_subnet(Node_t *n);

void process_flow(const struct pcap_pkthdr *h, uint8_t version_ip, ip_union_t *src_ip, ip_union_t *dst_ip, uint16_t src_port, uint16_t dst_port, uint8_t protocol);

void sigproc(int sig);
int drop_privileges_capabilities(void);
void packetProcessHandler(u_char *dev_idx, const struct pcap_pkthdr *h, const u_char *p);
char* etheraddr_string(const u_char *ep, char *buf);
void free_resources();


/* SIGINT/SIGTERM: interrompe il pcap_loop in corso. */
void sigproc(int sig) {
    static int called = 0;

    fprintf(stderr, "Uscita in corso...\n");
    if (called) return;
    else called = 1;

    for (int i = 0; i < NUM_CAPTURE; i++) {
        if (pd_array[i] != NULL) {
            pcap_breakloop(pd_array[i]);
        }
    }
}

/* Riduce i privilegi alle sole capability necessarie e passa a 'nobody'. */
int drop_privileges_capabilities(void) {
    cap_t caps;
    cap_value_t cap_list[] = {
        CAP_DAC_OVERRIDE,       /* lettura dei file pcap */
        CAP_DAC_READ_SEARCH     /* lettura delle directory */
    };
    int num_caps = sizeof(cap_list) / sizeof(cap_list[0]);

    /* Mantiene le capability dopo setuid() */
    if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0) {
        fprintf(stderr, "Failed to set keepcaps: %s\n", strerror(errno));
        return -1;
    }

    /* Capability correnti */
    caps = cap_get_proc();
    if (caps == NULL) {
        fprintf(stderr, "Failed to get current capabilities: %s\n", strerror(errno));
        return -1;
    }

    /* Azzera tutto */
    if (cap_clear(caps) < 0) {
        fprintf(stderr, "Failed to clear capabilities: %s\n", strerror(errno));
        cap_free(caps);
        return -1;
    }

    /* Solo quelle necessarie */
    if (cap_set_flag(caps, CAP_EFFECTIVE, num_caps, cap_list, CAP_SET) < 0 ||
        cap_set_flag(caps, CAP_PERMITTED, num_caps, cap_list, CAP_SET) < 0) {
        fprintf(stderr, "Failed to set capability flags: %s\n", strerror(errno));
        cap_free(caps);
        return -1;
    }

    /* Applica */
    if (cap_set_proc(caps) < 0) {
        fprintf(stderr, "Failed to set process capabilities: %s\n", strerror(errno));
        cap_free(caps);
        return -1;
    }

    cap_free(caps);

    /* Passa a 'nobody' mantenendo le capability */
    struct passwd *pw = getpwnam("nobody");
    if (pw != NULL) {
        if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
            fprintf(stderr, "Warning: unable to change to user 'nobody': %s\n", strerror(errno));
            fprintf(stderr, "Continuing with root user but limited capabilities\n");
        } else {
            fprintf(stderr, "Changed to user 'nobody' with restricted capabilities\n");
        }
    }

    fprintf(stderr, "Privileges dropped successfully. Retained capabilities:\n");
    caps = cap_get_proc();
    if (caps != NULL) {
        char *cap_text = cap_to_text(caps, NULL);
        fprintf(stderr, "  %s\n", cap_text);
        cap_free(cap_text);
        cap_free(caps);
    }
    fprintf(stderr, "\n");

    umask(0);
    return 0;
}

/* ---------- Gestione della tabella hash dei nodi ---------- */

/* FNV-1a a 32 bit sulla stringa del MAC, ridotto ai bucket. */
static uint32_t mac_hash(const char *mac) {
    uint32_t h = 2166136261u;   /* FNV-1a offset basis */
    for (const unsigned char *s = (const unsigned char *)mac; *s != '\0'; s++) {
        h ^= (uint32_t)(*s);
        h *= 16777619u;         /* FNV prime */
    }
    return h & (NODE_HASH_SIZE - 1);
}

/* Nodo con questo MAC, NULL se mai visto. */
Node_t *node_lookup(const char *mac) {
    for (Node_t *e = node_table[mac_hash(mac)]; e != NULL; e = e->next) {
        if (strcmp(e->mac, mac) == 0) {
            return e;
        }
    }
    return NULL;
}

/* Nodo con questo MAC, creato se assente; NULL solo se manca memoria. */
static Node_t *node_get_or_create(const char *mac) {
    uint32_t idx = mac_hash(mac);

    for (Node_t *e = node_table[idx]; e != NULL; e = e->next) {
        if (strcmp(e->mac, mac) == 0) return e;
    }

    Node_t *e = calloc(1, sizeof(Node_t));
    if (e == NULL) return NULL;

    e->seen = calloc((size_t)NUM_CAPTURE, sizeof(uint8_t));
    if (e->seen == NULL) {
        free(e);
        return NULL;
    }

    strncpy(e->mac, mac, sizeof(e->mac) - 1);
    e->subnet_idx = -1; /* nessuna sottorete finche' non c'e' un IP */

    /* Aggiunta a node_list (capacita' raddoppiata quando piena) */
    if (total_nodes == node_list_cap) {
        int new_cap = (node_list_cap > 0) ? node_list_cap * 2 : 256;
        Node_t **tmp = realloc(node_list, (size_t)new_cap * sizeof(*tmp));
        if (tmp == NULL) {
            free(e->seen);
            free(e);
            return NULL;
        }
        node_list = tmp;
        node_list_cap = new_cap;
    }
    node_list[total_nodes++] = e;

    e->next = node_table[idx]; /* inserimento in testa */
    node_table[idx] = e;
    return e;
}

/* Ritorna 1 se l'IPv4 e' privato o link-local, mentre pubblici e multicast restano esclusi. */
int is_private_or_local_ip(uint32_t ip_net) {
    uint32_t ip = ntohl(ip_net); /* host order: primo ottetto = byte alto */
    uint8_t o1 = (uint8_t)((ip >> 24) & 0xFF);
    uint8_t o2 = (uint8_t)((ip >> 16) & 0xFF);

    /* 10.0.0.0/8 */
    if (o1 == 10) return 1;
    /* 172.16.0.0/12 */
    if (o1 == 172 && (o2 >= 16 && o2 <= 31)) return 1;
    /* 192.168.0.0/16 */
    if (o1 == 192 && o2 == 168) return 1;
    /* 169.254.0.0/16 (link-local) */
    if (o1 == 169 && o2 == 254) return 1;
    /* altrimenti pubblico o multicast */
    return 0;
}

/* 1 se l'indirizzo e' ammesso come nodo della rete osservata. */
int is_node_valid(uint8_t version_ip, const ip_union_t *ip) {
    if (version_ip == 4) {
        uint32_t host = ntohl(ip->v4);

        /* 0.0.0.0 */
        if (host == 0x00000000u) return 0;
        /* 255.255.255.255 */
        if (host == 0xFFFFFFFFu) return 0;
        /* 127.0.0.0/8 */
        if (((host >> 24) & 0xFF) == 127) return 0;

        return is_private_or_local_ip(ip->v4);
    } else if (version_ip == 6) {
        const struct in6_addr *a = &ip->v6;

        /* :: */
        if (IN6_IS_ADDR_UNSPECIFIED(a)) return 0;
        /* ::1 */
        if (IN6_IS_ADDR_LOOPBACK(a)) return 0;

        /* Solo link-local (fe80::/10) e unique-local (fc00::/7) */
        if (IN6_IS_ADDR_LINKLOCAL(a)) return 1;
        if ((a->s6_addr[0] & 0xFE) == 0xFC) return 1;

        return 0;
    }
    return 0;
}

/* "rete/prefisso" di un IPv4. */
void ipv4_to_subnet(uint32_t ip_net, int prefix_len, char *out, size_t out_len) {
    uint32_t ip_host = ntohl(ip_net);
    uint32_t mask = (prefix_len <= 0) ? 0u : (prefix_len >= 32 ? 0xFFFFFFFFu : (~0u << (32 - prefix_len)));
    uint32_t network = ip_host & mask;

    struct in_addr net_addr;
    net_addr.s_addr = htonl(network);

    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &net_addr, buf, sizeof(buf));
    snprintf(out, out_len, "%s/%d", buf, prefix_len);
}

/* "rete/prefisso" di un IPv6. */
void ipv6_to_subnet(const struct in6_addr *addr_in, int prefix_len, char *out, size_t out_len) {
    struct in6_addr addr = *addr_in;

    if (prefix_len < 0) {
        prefix_len = 0;
    }
    if (prefix_len > 128) {
        prefix_len = 128;
    }

    int full_bytes = prefix_len / 8;
    int rem_bits = prefix_len % 8;

    for (int i = full_bytes + (rem_bits ? 1 : 0); i < 16; i++) {
        addr.s6_addr[i] = 0;
    }

    if (rem_bits) {
        uint8_t keep_mask = (uint8_t)(0xFF << (8 - rem_bits));
        addr.s6_addr[full_bytes] &= keep_mask;
    }

    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
    snprintf(out, out_len, "%s/%d", buf, prefix_len);
}

/* Ultimo indirizzo (broadcast) di una sottorete IPv4. */
void ipv4_last_addr_from_network(const char *network_str, int prefix_len, char *out, size_t out_len) {
    struct in_addr addr;
    if (inet_pton(AF_INET, network_str, &addr) != 1) {
        snprintf(out, out_len, "INVALID");
        return;
    }

    uint32_t network = ntohl(addr.s_addr);
    uint32_t mask = (prefix_len <= 0) ? 0u : (prefix_len >= 32 ? 0xFFFFFFFFu : (~0u << (32 - prefix_len)));
    uint32_t broadcast = network | (~mask);

    struct in_addr last_addr;
    last_addr.s_addr = htonl(broadcast);
    inet_ntop(AF_INET, &last_addr, out, out_len);
}

/* Ultimo indirizzo di una sottorete IPv6. */
void ipv6_last_addr_from_network(const char *network_str, int prefix_len, char *out, size_t out_len) {
    struct in6_addr addr;
    if (inet_pton(AF_INET6, network_str, &addr) != 1) {
        snprintf(out, out_len, "INVALID");
        return;
    }

    if (prefix_len < 0) {
        prefix_len = 0;
    }
    if (prefix_len > 128) {
        prefix_len = 128;
    }

    int full_bytes = prefix_len / 8;
    int rem_bits = prefix_len % 8;

    for (int i = full_bytes + (rem_bits ? 1 : 0); i < 16; i++) {
        addr.s6_addr[i] = 0xFF;
    }

    if (rem_bits) {
        uint8_t host_mask = (uint8_t)(0xFF >> rem_bits);
        addr.s6_addr[full_bytes] |= host_mask;
    }

    inet_ntop(AF_INET6, &addr, out, out_len);
}

/* Indice della sottorete, creata se assente; -1 oltre MAX_SUBNETS. */
int create_subnet(uint8_t version_ip, const char *subnet_str, int prefix_len) {
    for (int i = 0; i < total_subnets; i++) {
        if (subnet_stats[i].version_ip == version_ip &&
            strcmp(subnet_stats[i].subnet, subnet_str) == 0) {
            return i;
        }
    }

    if (total_subnets >= MAX_SUBNETS) {
        fprintf(stderr, "Attenzione: Massimo numero di sottoreti raggiunto.\n");
        return -1;
    }

    strncpy(subnet_stats[total_subnets].subnet, subnet_str, SUBNET_STR_LEN - 1);
    subnet_stats[total_subnets].subnet[SUBNET_STR_LEN - 1] = '\0';
    subnet_stats[total_subnets].version_ip = version_ip;
    subnet_stats[total_subnets].prefix_len = prefix_len;
    subnet_stats[total_subnets].node_count = 0;
    subnet_stats[total_subnets].flow_count = 0;

    /* Ultimo indirizzo, per stampare il range */
    char network_only[INET6_ADDRSTRLEN];
    const char *slash = strchr(subnet_str, '/');
    size_t net_len = slash ? (size_t)(slash - subnet_str) : strlen(subnet_str);
    if (net_len >= sizeof(network_only)) {
        net_len = sizeof(network_only) - 1;
    }
    memcpy(network_only, subnet_str, net_len);
    network_only[net_len] = '\0';

    if (version_ip == 4) {
        ipv4_last_addr_from_network(network_only, prefix_len, subnet_stats[total_subnets].range_end, sizeof(subnet_stats[total_subnets].range_end));
    } else {
        ipv6_last_addr_from_network(network_only, prefix_len, subnet_stats[total_subnets].range_end, sizeof(subnet_stats[total_subnets].range_end));
    }

    return total_subnets++;
}

/* Unica sottorete del nodo secondo NODE_SUBNET_PREFER_IPV4; -1 se non ha IP. */
int node_compute_subnet(const Node_t *n) {
    char subnet_str[SUBNET_STR_LEN];

#if NODE_SUBNET_PREFER_IPV4
    if (n->has_ipv4) {
        ipv4_to_subnet(n->ipv4, IPV4_SUBNET_PREFIX, subnet_str, sizeof(subnet_str));
        return create_subnet(4, subnet_str, IPV4_SUBNET_PREFIX);
    }
    if (n->has_ipv6) {
        ipv6_to_subnet(&n->ipv6, IPV6_SUBNET_PREFIX, subnet_str, sizeof(subnet_str));
        return create_subnet(6, subnet_str, IPV6_SUBNET_PREFIX);
    }
#else
    if (n->has_ipv6) {
        ipv6_to_subnet(&n->ipv6, IPV6_SUBNET_PREFIX, subnet_str, sizeof(subnet_str));
        return create_subnet(6, subnet_str, IPV6_SUBNET_PREFIX);
    }
    if (n->has_ipv4) {
        ipv4_to_subnet(n->ipv4, IPV4_SUBNET_PREFIX, subnet_str, sizeof(subnet_str));
        return create_subnet(4, subnet_str, IPV4_SUBNET_PREFIX);
    }
#endif
    return -1;
}

/* Ri-assegna il nodo alla sua sottorete aggiornando node_count. Serve quando un nodo visto prima solo in IPv6 rivela un IPv4: passa di sottorete senza essere contato due volte. */
void node_update_subnet(Node_t *n) {
    int new_idx = node_compute_subnet(n);

    /* invariata */
    if (new_idx == n->subnet_idx) {
        return;
    }

    /* esce dalla vecchia */
    if (n->subnet_idx >= 0) {
        subnet_stats[n->subnet_idx].node_count--;
    }

    /* entra nella nuova */
    if (new_idx >= 0) {
        subnet_stats[new_idx].node_count++;
    }

    n->subnet_idx = new_idx;
}

void save_node(int capture_idx, const char *mac, uint8_t version_ip, ip_union_t *ip) {
    if (!is_node_valid(version_ip, ip)) {
        return; /* indirizzo speciale o pubblico: nessuna cattura */
    }

    Node_t *n = node_get_or_create(mac);
    if (n == NULL) {
        fprintf(stderr, "Attenzione: memoria esaurita, impossibile registrare il nodo %s.\n", mac);
        return;
    }

    /* Si conserva solo il primo indirizzo per famiglia: un nodo dual-stack resta un solo individuo, attribuito a una sola sottorete. */
    int addr_changed = 0;

    if (version_ip == 4 && !n->has_ipv4) {
        n->ipv4 = ip->v4;
        n->has_ipv4 = 1;
        addr_changed = 1;
    } else if (version_ip == 6 && !n->has_ipv6) {
        n->ipv6 = ip->v6;
        n->has_ipv6 = 1;
        addr_changed = 1;
    }

    if (addr_changed) {
        node_update_subnet(n);
    }

    n->seen[capture_idx] = 1;
}

/* ---------- Gestione della tabella hash dei flussi ---------- */

/* FNV-1a a 64 bit campo per campo, per non leggere il padding della struct.
 * Le union IP sono azzerate prima dell'uso, quindi i 16 byte sono definiti. */
static uint64_t flow_hash(const FlowKey_t *k) {
    uint64_t h = 1469598103934665603ULL;      /* FNV-1a offset basis */
    const uint64_t prime = 1099511628211ULL;
    #define FNV_BYTE(b) do { h ^= (uint8_t)(b); h *= prime; } while (0)

    FNV_BYTE(k->version_ip);
    FNV_BYTE(k->protocol);
    FNV_BYTE(k->src_port & 0xFF);
    FNV_BYTE((k->src_port >> 8) & 0xFF);
    FNV_BYTE(k->dst_port & 0xFF);
    FNV_BYTE((k->dst_port >> 8) & 0xFF);

    const uint8_t *s = (const uint8_t *)&k->src_ip;
    const uint8_t *d = (const uint8_t *)&k->dst_ip;
    for (int i = 0; i < (int)sizeof(ip_union_t); i++) {
        FNV_BYTE(s[i]);
    }
    for (int i = 0; i < (int)sizeof(ip_union_t); i++) {
        FNV_BYTE(d[i]);
    }

    #undef FNV_BYTE
    return h;
}

/* Uguaglianza fra due 5-tuple. */
static int flow_key_equal(const FlowKey_t *a, const FlowKey_t *b) {
    return a->version_ip == b->version_ip &&
           a->protocol == b->protocol &&
           a->src_port == b->src_port &&
           a->dst_port == b->dst_port &&
           memcmp(&a->src_ip, &b->src_ip, sizeof(ip_union_t)) == 0 &&
           memcmp(&a->dst_ip, &b->dst_ip, sizeof(ip_union_t)) == 0;
}

/* Flusso creato se assente (*is_new = 1); NULL solo se manca memoria. */
static FlowEntry_t *flow_get_or_create(const FlowKey_t *key, int *is_new) {
    uint32_t idx = (uint32_t)(flow_hash(key) & (FLOW_HASH_SIZE - 1));

    for (FlowEntry_t *e = flow_table[idx]; e != NULL; e = e->next) {
        if (flow_key_equal(&e->key, key)) {
            *is_new = 0;
            return e;
        }
    }

    FlowEntry_t *e = calloc(1, sizeof(FlowEntry_t));
    if (e == NULL) {
        *is_new = 0;
        return NULL;
    }

    e->key = *key;
    e->subnet_idx = -1;
    e->next = flow_table[idx];  /* inserimento in testa */
    flow_table[idx] = e;
    total_flows++;
    *is_new = 1;
    return e;
}

/* now - before in microsecondi. */
static long delta_time_usec(const struct timeval *now, const struct timeval *before) {
    long delta_seconds = (long)(now->tv_sec - before->tv_sec);
    long delta_usec = (long)(now->tv_usec - before->tv_usec);

    if (delta_usec < 0) {
        delta_usec += 1000000;
        --delta_seconds;
    }
    return (delta_seconds * 1000000) + delta_usec;
}

void process_flow(const struct pcap_pkthdr *h, uint8_t version_ip, ip_union_t *src_ip, ip_union_t *dst_ip, uint16_t src_port, uint16_t dst_port, uint8_t protocol) {
    if (version_ip == 0) {
        return;
    }

    if (!is_node_valid(version_ip, src_ip)) { /* sorgente non ammessa: flusso ignorato */
        return;
    }

    FlowKey_t key;
    memset(&key, 0, sizeof(key));   /* azzera anche i byte inutilizzati delle union */
    key.version_ip = version_ip;
    key.src_ip = *src_ip;
    key.dst_ip = *dst_ip;
    key.src_port = src_port;
    key.dst_port = dst_port;
    key.protocol = protocol;

    int is_new = 0;
    FlowEntry_t *fe = flow_get_or_create(&key, &is_new);
    if (fe == NULL) {
        fprintf(stderr, "Attenzione: memoria esaurita, impossibile registrare nuovi flussi.\n");
        return;
    }

    /* Contatori aggiornati a ogni pacchetto */
    fe->packets++;
    fe->bytes += h->len;
    fe->last_ts = h->ts;

    if (!is_new) { /* flusso gia' contato */
        return;
    }
    fe->first_ts = h->ts;

    char subnet_str[SUBNET_STR_LEN];
    int prefix_len;
    if (version_ip == 4) {
        prefix_len = IPV4_SUBNET_PREFIX;
        ipv4_to_subnet(src_ip->v4, prefix_len, subnet_str, sizeof(subnet_str));
    } else {
        prefix_len = IPV6_SUBNET_PREFIX;
        ipv6_to_subnet(&src_ip->v6, prefix_len, subnet_str, sizeof(subnet_str));
    }

    int idx = create_subnet(version_ip, subnet_str, prefix_len);
    if (idx >= 0) {
        fe->subnet_idx = idx;
        subnet_stats[idx].flow_count++;
    }
}

/* Elabora un pacchetto: estrae MAC e IP dei due estremi e li registra. */
void packetProcessHandler(u_char *dev_idx, const struct pcap_pkthdr *h, const u_char *p) {
    int capture_idx = (dev_idx != NULL) ? *(int *)dev_idx : 0;

    /* ========== Campi della 5-tupla ========== */
    uint8_t version_ip = 0;
    ip_union_t src_ip = {0};
    ip_union_t dst_ip = {0};
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    uint8_t protocol = 0;
    uint16_t icmp_type = 0;   /* al posto della porta sorgente per ICMP */
    uint16_t icmp_code = 0;   /* al posto della porta destinazione per ICMP */

    /* ========== Header pcap ========== */
    u_int incl_len = h->caplen;         /* byte catturati */
    u_int orig_len = h->len;            /* lunghezza originale */

    if (incl_len > orig_len) {
        fprintf(stderr, "[ERRORE] Pacchetto corrotto: caplen (%u) > len (%u).\n", incl_len, orig_len);
        return;
    }

    /* ========== Livello 2: Ethernet ========== */
    if (incl_len < sizeof(struct ether_header)) {
        return;
    }

    struct ether_header ehdr;
    u_short eth_type;                   /* 0x0800 IPv4, 0x86DD IPv6, 0x0806 ARP */
    u_char eth_dhost[ETHER_ADDR_LEN];   /* MAC destinazione */
	u_char eth_shost[ETHER_ADDR_LEN];   /* MAC sorgente */
    memcpy(&ehdr, p, sizeof(struct ether_header));
    memcpy(eth_dhost, ehdr.ether_dhost, ETHER_ADDR_LEN);
    memcpy(eth_shost, ehdr.ether_shost, ETHER_ADDR_LEN);

    eth_type = ntohs(ehdr.ether_type);

    char smac[18], dmac[18];
    etheraddr_string(ehdr.ether_shost, smac);
	etheraddr_string(ehdr.ether_dhost, dmac);

    /* ========== Livello 3: IPv4, IPv6, ARP ========== */
    if (eth_type == ETHERTYPE_IP) {
        /* IPv4 */
        if (incl_len - sizeof(struct ether_header) < (int)sizeof(struct ip)) {
            return;
        }

        version_ip = 4;
        struct ip iph;
        memcpy(&iph, p + sizeof(struct ether_header), sizeof(struct ip));

        src_ip.v4 = iph.ip_src.s_addr;   /* network byte order */
        dst_ip.v4 = iph.ip_dst.s_addr;
        protocol = iph.ip_p;

        /* ========== Livello 4: TCP, UDP, ICMP ========== */
        if (protocol == IPPROTO_TCP || protocol == IPPROTO_UDP || protocol == IPPROTO_ICMP) {
            int ip_hlen = (iph.ip_v & 0x0f) * 4;
            int payload_offset = sizeof(struct ether_header) + ip_hlen;

            if (protocol == IPPROTO_TCP) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip) + sizeof(struct tcphdr))) {
                    return;
                }

                struct tcphdr tcph;
                memcpy(&tcph, p + sizeof(struct ether_header) + iph.ip_hl * 4, sizeof(struct tcphdr));

                if (incl_len >= payload_offset + sizeof(struct tcphdr)) {
                    src_port = ntohs(tcph.th_sport);
                    dst_port = ntohs(tcph.th_dport);
                }

            } else if (protocol == IPPROTO_UDP) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip) + sizeof(struct udphdr))) {
                    return;
                }

                struct udphdr udph;
                memcpy(&udph, p + sizeof(struct ether_header) + iph.ip_hl * 4, sizeof(struct udphdr));

                if (incl_len >= payload_offset + sizeof(struct udphdr)) {
                    src_port = ntohs(udph.uh_sport);
                    dst_port = ntohs(udph.uh_dport);
                }

            } else if (protocol == IPPROTO_ICMP) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip) + sizeof(struct icmphdr))) {
                    return;
                }

                struct icmphdr icmph;
                memcpy(&icmph, p + sizeof(struct ether_header) + iph.ip_hl * 4, sizeof(struct icmphdr));

                if (incl_len >= payload_offset + sizeof(struct icmphdr)) {
                    icmp_type = icmph.type;
                    icmp_code = icmph.code;
                }

                if ((icmp_type == 11 || icmp_type == 3) && icmp_code == 0) {
                    return; /* errori di instradamento (rete irraggiungibile, TTL scaduto): scartati */
                }

            }
        } else {
            /* Altri protocolli: nodi registrati, nessun flusso */
        }
    } else if (eth_type == ETHERTYPE_IPV6) {
        /* IPv6 */
        if (incl_len - sizeof(struct ether_header) < (int)sizeof(struct ip6_hdr)) {
            return;
        }

        version_ip = 6;
        struct ip6_hdr ip6h;
        memcpy(&ip6h, p + sizeof(struct ether_header), sizeof(struct ip6_hdr));

        src_ip.v6 = ip6h.ip6_src;
        dst_ip.v6 = ip6h.ip6_dst;
        protocol = ip6h.ip6_nxt;

        /* ========== Livello 4: TCP, UDP, ICMPv6 (solo next header fisso) ========== */
        if (protocol == IPPROTO_TCP || protocol == IPPROTO_UDP || protocol == IPPROTO_ICMPV6) {
            int ip6_hlen = sizeof(struct ip6_hdr);
            int payload_offset = sizeof(struct ether_header) + ip6_hlen;

            if (protocol == IPPROTO_TCP) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip6_hdr) + sizeof(struct tcphdr))) {
                    return;
                }

                struct tcphdr tcph;
                memcpy(&tcph, p + payload_offset, sizeof(struct tcphdr));

                if (incl_len >= payload_offset + sizeof(struct tcphdr)) {
                    src_port = ntohs(tcph.th_sport);
                    dst_port = ntohs(tcph.th_dport);
                }

            } else if (protocol == IPPROTO_UDP) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip6_hdr) + sizeof(struct udphdr))) {
                    return;
                }

                struct udphdr udph;
                memcpy(&udph, p + payload_offset, sizeof(struct udphdr));

                if (incl_len >= payload_offset + sizeof(struct udphdr)) {
                    src_port = ntohs(udph.uh_sport);
                    dst_port = ntohs(udph.uh_dport);
                }

            } else if (protocol == IPPROTO_ICMPV6) {
                if (incl_len - sizeof(struct ether_header) < (int)(sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr))) {
                    return;
                }

                struct icmp6_hdr icmp6h;
                memcpy(&icmp6h, p + payload_offset, sizeof(struct icmp6_hdr));

                if (incl_len >= payload_offset + sizeof(struct icmp6_hdr)) {
                    icmp_type = icmp6h.icmp6_type;
                    icmp_code = icmp6h.icmp6_code;
                }

                if ((icmp_type == 11 || icmp_type == 3) && icmp_code == 0) {
                    return; /* tipo 3 o 11 con codice 0: scartati */
                }

            }
        } else {
            /* Altri protocolli: nodi registrati, nessun flusso */
        }
    } else if (eth_type == ETHERTYPE_ARP) {
        /* ARP: IP dal payload (sender/target protocol address) */
        if (incl_len - sizeof(struct ether_header) < (int)sizeof(struct ether_arp)) {
            return;
        }

        version_ip = 4;
        struct ether_arp arph;
        memcpy(&arph, p + sizeof(struct ether_header), sizeof(struct ether_arp));

        memcpy(&src_ip.v4, arph.arp_spa, 4);   /* gia' network byte order */
        memcpy(&dst_ip.v4, arph.arp_tpa, 4);

        uint16_t op = ntohs(arph.arp_op);

        /* ARP come pseudo-flusso: protocollo fittizio e opcode al posto della porta. */
        protocol = ARP_PSEUDO_PROTOCOL;
        src_port = op;
        dst_port = 0;

        if (op == ARPOP_REQUEST) {
            /* request: nessuna azione */
        } else if (op == ARPOP_REPLY) {
            /* reply: nessuna azione */
        }

    } else {
        /* Ethertype non gestito: pacchetto ignorato */
        return;
    }

    if (version_ip != 0) {
        save_node(capture_idx, smac, version_ip, &src_ip);
        save_node(capture_idx, dmac, version_ip, &dst_ip);

        /* Flusso attribuito alla sottorete della sorgente; ICMP usa (tipo, codice) al posto delle porte. */
        if (protocol == IPPROTO_TCP || protocol == IPPROTO_UDP || protocol == ARP_PSEUDO_PROTOCOL) {
            process_flow(h, version_ip, &src_ip, &dst_ip, src_port, dst_port, protocol);
        } else if (protocol == IPPROTO_ICMP || protocol == IPPROTO_ICMPV6) {
            process_flow(h, version_ip, &src_ip, &dst_ip, icmp_type, icmp_code, protocol);
        }
    }
}

/* Nome del protocollo, ARP incluso. */
static const char *proto_name(uint8_t proto) {
    static char proto_buf[16];
    switch (proto) {
        case IPPROTO_TCP: return "TCP";
        case IPPROTO_UDP: return "UDP";
        case IPPROTO_ICMP: return "ICMP";
        case IPPROTO_ICMPV6: return "ICMPv6";
        case ARP_PSEUDO_PROTOCOL: return "ARP";
        default:
            snprintf(proto_buf, sizeof(proto_buf), "%u", proto);
            return proto_buf;
    }
}

/* Stampa i flussi: 5-tupla, pacchetti, byte e durata. Non chiamata dal main. */
void flow_table_print_stats() {
    if (total_flows == 0) {
        return;
    }

    fprintf(stdout, "\n=============== TABELLA DEI FLUSSI (HASH TABLE) ===============\n");
    fprintf(stdout, "%-47s | %-47s | %-8s | %-10s | %-12s | %-12s\n", "Sorgente", "Destinazione", "Proto", "Pacchetti", "Byte", "Durata (s)");
    fprintf(stdout, "----------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    double total_duration = 0.0;
    uint64_t total_packets = 0, total_bytes = 0;

    for (int i = 0; i < FLOW_HASH_SIZE; i++) {
        for (FlowEntry_t *e = flow_table[i]; e != NULL; e = e->next) {
            char src_buf[INET6_ADDRSTRLEN], dst_buf[INET6_ADDRSTRLEN];
            char src_str[INET6_ADDRSTRLEN + 8], dst_str[INET6_ADDRSTRLEN + 8];

            if (e->key.version_ip == 4) {
                inet_ntop(AF_INET, &e->key.src_ip.v4, src_buf, sizeof(src_buf));
                inet_ntop(AF_INET, &e->key.dst_ip.v4, dst_buf, sizeof(dst_buf));
            } else {
                inet_ntop(AF_INET6, &e->key.src_ip.v6, src_buf, sizeof(src_buf));
                inet_ntop(AF_INET6, &e->key.dst_ip.v6, dst_buf, sizeof(dst_buf));
            }

            snprintf(src_str, sizeof(src_str), "%s:%u", src_buf, e->key.src_port);
            snprintf(dst_str, sizeof(dst_str), "%s:%u", dst_buf, e->key.dst_port);

            double duration = (double)delta_time_usec(&e->last_ts, &e->first_ts) / 1000000.0;
            total_duration += duration;
            total_packets += e->packets;
            total_bytes += e->bytes;

            fprintf(stdout, "%-47s | %-47s | %-8s | %-10llu | %-12llu | %.6f\n",
                    src_str, dst_str, proto_name(e->key.protocol), (unsigned long long)e->packets, (unsigned long long)e->bytes, duration);
        }
    }

    fprintf(stdout, "----------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    fprintf(stdout, "Flussi unici totali: %d\n", total_flows);
    fprintf(stdout, "Pacchetti totali attribuiti a flussi: %llu\n", (unsigned long long)total_packets);
    fprintf(stdout, "Byte totali attribuiti a flussi: %llu\n", (unsigned long long)total_bytes);
    fprintf(stdout, "Durata media di un flusso: %.6f s\n", total_duration / (double)total_flows);
}

static void estimator_data_free(EstimatorData_t *ed) {
    free(ed->seen);
    free(ed->subnet_idx);
    free(ed->subnet_version);
    free(ed->subnet_name);
    free(ed->subnet_range_end);
    free(ed->subnet_flow_count);
    memset(ed, 0, sizeof(*ed));
}

/* Vista dei dati per gli stimatori (puntatori, nessuna copia). 0 se manca
 * memoria, con ed gia' ripulito. */
static int estimator_data_build(EstimatorData_t *ed) {
    memset(ed, 0, sizeof(*ed));
    ed->t = NUM_CAPTURE;
    ed->n_nodes = total_nodes;
    ed->n_subnets = total_subnets;

    /* Almeno un elemento: malloc(0) puo' restituire NULL senza errore. */
    size_t nn = (total_nodes > 0) ? (size_t)total_nodes : 1;
    size_t ns = (total_subnets > 0) ? (size_t)total_subnets : 1;

    ed->seen = malloc(nn * sizeof(*ed->seen));
    ed->subnet_idx = malloc(nn * sizeof(*ed->subnet_idx));
    ed->subnet_version = malloc(ns * sizeof(*ed->subnet_version));
    ed->subnet_name = malloc(ns * sizeof(*ed->subnet_name));
    ed->subnet_range_end = malloc(ns * sizeof(*ed->subnet_range_end));
    ed->subnet_flow_count = malloc(ns * sizeof(*ed->subnet_flow_count));

    if (ed->seen == NULL || ed->subnet_idx == NULL || ed->subnet_version == NULL || ed->subnet_name == NULL || ed->subnet_range_end == NULL || ed->subnet_flow_count == NULL) {
        estimator_data_free(ed);
        return 0;
    }

    for (int i = 0; i < total_nodes; i++) {
        ed->seen[i] = node_list[i]->seen;
        ed->subnet_idx[i] = node_list[i]->subnet_idx;
    }

    for (int s = 0; s < total_subnets; s++) {
        ed->subnet_version[s] = subnet_stats[s].version_ip;
        ed->subnet_name[s] = subnet_stats[s].subnet;
        ed->subnet_range_end[s] = subnet_stats[s].range_end;
        ed->subnet_flow_count[s] = subnet_stats[s].flow_count;
    }

    return 1;
}

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		fprintf(stderr, "Uso: %s <pcap_file1> <pcap_file2> <pcap_file3> ...\n", argv[0]);
		return 1;
	}

    NUM_CAPTURE = argc - 1;

    captures = calloc((size_t)NUM_CAPTURE, sizeof(*captures));
    pd_array = calloc((size_t)NUM_CAPTURE, sizeof(*pd_array));
    if (captures == NULL || pd_array == NULL) {
        fprintf(stderr, "Attenzione: allocazione memoria fallita per le catture\n");
        free_resources();
        return 1;
    }

    for (int i = 0; i < NUM_CAPTURE; i++) {
        captures[i] = strdup(argv[i + 1]);
        if (captures[i] == NULL) {
            fprintf(stderr, "Attenzione: allocazione memoria fallita per il nome '%s'\n", argv[i + 1]);
            free_resources();
            return 1;
        }
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    struct stat st;

    if (geteuid() != 0) {
        fprintf(stderr, "Please run this tool as superuser (or with CAP_NET_RAW and CAP_NET_ADMIN capabilities)\n");
        free_resources();
        return -1;
    }

    for (int i = 0; i < NUM_CAPTURE; i++) {
        fprintf(stdout, "Inizializzazione cattura da: %s\n", captures[i]);

        if (stat(captures[i], &st) != 0) {
            fprintf(stderr, "File %s non trovato o non accessibile: %s\n", captures[i], strerror(errno));
            free_resources();
            return -1;
        }

        pd_array[i] = pcap_open_offline(captures[i], errbuf);
        if (pd_array[i] == NULL) {
            fprintf(stderr, "pcap_open_offline per %s fallito: %s\n", captures[i], errbuf);
            free_resources();
            return -1;
        }
    }

    /* Riduzione dei privilegi */
    if (drop_privileges_capabilities() < 0) {
        fprintf(stderr, "Warning: Failed to drop privileges with capabilities\n");
        fprintf(stderr, "Continuing with full privileges\n");
    }

    /* Segnali per l'uscita controllata */
    signal(SIGINT, sigproc);
    signal(SIGTERM, sigproc);

    /* Elaborazione delle catture */
    for (int i = 0; i < NUM_CAPTURE; i++) {
        int current_capture = i;
        if (pcap_loop(pd_array[i], -1, packetProcessHandler, (u_char *)&current_capture) < 0) {
            char *err_msg = pcap_geterr(pd_array[i]);
            /* File troncato: avviso, si prosegue con i pacchetti letti */
            if (strstr(err_msg, "truncated") != NULL || strstr(err_msg, "cut short") != NULL) {
                fprintf(stderr, "[WARNING: %s] Il file %s si è interrotto bruscamente (file troncato). I pacchetti precedenti sono stati elaborati.\n", err_msg, captures[i]);
            } else {
                /* Altri errori */
                fprintf(stderr, "Errore critico nel loop di cattura per %s: %s\n", captures[i], err_msg);
            }
        } else {
            fprintf(stdout, "Cattura da %s completata con successo.\n", captures[i]);
        }
    }

    /* Stime */
    EstimatorData_t edata;
    if (estimator_data_build(&edata)) {
        node_subnet_capture_probability_print_stats(&edata);
        node_jackknife_estimators_print_stats(&edata);
        node_chao_estimators_print_stats(&edata);
        node_huggins_estimators_print_stats(&edata);
        estimator_data_free(&edata);
    } else {
        fprintf(stderr, "Memoria insufficiente: stime non calcolate.\n");
    }

    free_resources();

    return 0;
}

/* MAC in stringa "XX:XX:XX:XX:XX:XX" (buf di almeno 18 byte). */
char* etheraddr_string(const u_char *ep, char *buf) {
    u_int i, j;
    char *cp;

    cp = buf;
    if ((j = *ep >> 4) != 0) {
        *cp++ = hex[j];
    } else {
        *cp++ = '0';
    }

    *cp++ = hex[*ep++ & 0xf];

    for (i = 5; (int)--i >= 0;) {
        *cp++ = ':';
        if ((j = *ep >> 4) != 0) {
            *cp++ = hex[j];
        } else {
            *cp++ = '0';
        }
        *cp++ = hex[*ep++ & 0xf];
    }

    *cp = '\0';
    return (buf);
}

void free_resources() {
    if (pd_array != NULL) {
        for (int i = 0; i < NUM_CAPTURE; i++) {
            if (pd_array[i] != NULL) {
                pcap_close(pd_array[i]);
                pd_array[i] = NULL;
            }
        }
        free(pd_array);
        pd_array = NULL;
    }

    if (captures != NULL) {
        for (int i = 0; i < NUM_CAPTURE; i++) {
            free(captures[i]);
        }
        free(captures);
        captures = NULL;
    }

    /* Nodi, tramite node_list */
    for (int i = 0; i < total_nodes; i++) {
        free(node_list[i]->seen);
        free(node_list[i]);
    }
    free(node_list);
    node_list = NULL;
    node_list_cap = 0;
    total_nodes = 0;
    memset(node_table, 0, sizeof(node_table));

    /* Flussi, catena per catena */
    for (int i = 0; i < FLOW_HASH_SIZE; i++) {
        FlowEntry_t *e = flow_table[i];
        while (e != NULL) {
            FlowEntry_t *next = e->next;
            free(e);
            e = next;
        }
        flow_table[i] = NULL;
    }
    total_flows = 0;
}