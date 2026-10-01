import random
import ipaddress
from scapy.all import (
    Ether, IP, IPv6, TCP, UDP, ICMP, ICMPv6EchoRequest, ARP, BOOTP, DHCP, wrpcap
)

# Numero di server, sempre i primi nodi della popolazione.
NUM_SERVER = 30

# Blocchi privati e link-local da cui prendono l'indirizzo i client.
RETE_CLASSE_A = ipaddress.ip_network('10.0.0.0/8')
RETE_CLASSE_B = ipaddress.ip_network('172.16.0.0/12')
RETE_CLASSE_C = ipaddress.ip_network('192.168.0.0/16')
RETE_LINK_LOCAL = ipaddress.ip_network('169.254.0.0/16')

# Rete di ciascuna classe di client.
RETE_DI_CLASSE = {
    'CLASSE_A': RETE_CLASSE_A,
    'CLASSE_B': RETE_CLASSE_B,
    'CLASSE_C': RETE_CLASSE_C,
    'LINK_LOCAL': RETE_LINK_LOCAL,
}

# Ordine delle classi: i client sono disposti a blocchi, dopo i server.
ORDINE_CLASSI = ['CLASSE_A', 'CLASSE_B', 'CLASSE_C', 'LINK_LOCAL']

# --------------------------------------------------------------------------- #
# NUMERO DI CLIENT PER CLASSE
# La loro somma e' il numero di client. I server si aggiungono a parte.
# Negli esperimenti i client sono 1000, 500 o 100, ripartiti per il 30% in ciascuna delle classi A, B e C e per il restante 10% nella classe link-local.
# DEFAULT: 30 client per ciascuna classe A, B e C, 10 link-local, per un totale di 100 client.
# --------------------------------------------------------------------------- #
NODI_PER_CLASSE = {
    'CLASSE_A': 30,
    'CLASSE_B': 30,
    'CLASSE_C': 30,
    'LINK_LOCAL': 10
}

# --------------------------------------------------------------------------- #
# PROFILO DI TRAFFICO
#   - 'molto_frequente' : nodi presenti spesso, molte ricatture
#   - 'bilanciato'      : presenza intermedia
#   - 'poco_frequente'  : nodi presenti di rado, poche ricatture
# --------------------------------------------------------------------------- #
PROFILO_SELEZIONATO = 'poco_frequente'

# --------------------------------------------------------------------------- #
# RIPRODUCIBILITA'
#
# SEME_F       fissa le probabilita' di presenza dei nodi: a parita' di seme
#              la popolazione e la sua eterogeneita' restano le stesse.
# SEME_DATASET fissa tutto il resto (presenze per finestra, protocolli,
#              pacchetti, porte). Con entrambi i semi fissi i file sono identici
#              byte per byte; cambiando solo questo si ottiene una replica
#              indipendente della stessa popolazione.
# Ogni finestra usa un seme proprio, derivato da SEME_DATASET, giorno e fascia,
# cosi' rigenerare un file non altera gli altri.
# --------------------------------------------------------------------------- #
SEME_F = 1000
SEME_DATASET = 0

# Istante iniziale e durata di una fascia in secondi, per marche temporali deterministiche (altrimenti scapy userebbe l'orologio di sistema).
TS_INIZIO = 1700000000.0
DURATA_FASCIA = 8 * 3600
ORDINE_FASCE = ['mattina', 'pomeriggio', 'notte']

# Intervallo da cui si estrae, una volta sola, la probabilita' di presenza di ciascun nodo, per classe e profilo.
PROFILI_BENCHMARK = {
    'molto_frequente': {
        'SERVER':      (0.70, 0.90),
        'CLASSE_A':    (0.30, 0.50),
        'CLASSE_B':    (0.12, 0.25),
        'CLASSE_C':    (0.04, 0.10),
        'LINK_LOCAL':  (0.01, 0.04),
    },
    'bilanciato': {
        'SERVER':      (0.50, 0.75),
        'CLASSE_A':    (0.18, 0.32),
        'CLASSE_B':    (0.06, 0.14),
        'CLASSE_C':    (0.02, 0.06),
        'LINK_LOCAL':  (0.005, 0.02),
    },
    'poco_frequente': {
        'SERVER':      (0.30, 0.50),
        'CLASSE_A':    (0.08, 0.16),
        'CLASSE_B':    (0.03, 0.07),
        'CLASSE_C':    (0.01, 0.03),
        'LINK_LOCAL':  (0.00, 0.01),
    }
}

# Intervalli del profilo scelto.
CONFIG_PERCENTUALI = PROFILI_BENCHMARK[PROFILO_SELEZIONATO]

# Protocolli generabili (l'ordine fissa quello dei pesi).
LISTA_PROTOCOLLI = ['Ether', 'ARP', 'IP', 'IPv6', 'TCP', 'UDP', 'ICMP', 'ICMPv6EchoRequest', 'BOOTP', 'DHCP', 'SSDP', 'mDNS']


def costruisci_assegnazione_nodi(num_server=NUM_SERVER, nodi_per_classe=NODI_PER_CLASSE):
    """
    Restituisce la classe di ciascun nodo: prima i server, poi i client a blocchi.
    """
    classe_di_nodo = ['SERVER'] * num_server
    for nome_classe in ORDINE_CLASSI:
        classe_di_nodo += [nome_classe] * nodi_per_classe.get(nome_classe, 0)
    return classe_di_nodo


def pesi_da_percentuali(percentuali, lista_protocolli, contesto=""):
    """
    Converte le percentuali per protocollo in una lista di pesi nell'ordine di
    lista_protocolli; i protocolli assenti valgono zero.
    """
    sconosciuti = set(percentuali) - set(lista_protocolli)
    if sconosciuti:
        raise KeyError(
            f"Protocolli non riconosciuti in [{contesto}]: {sorted(sconosciuti)}. "
            f"Nomi validi: {lista_protocolli}"
        )

    return [percentuali.get(proto, 0) for proto in lista_protocolli]


def ip_da_rete(rete, offset):
    """
    Indirizzo IPv4 che si trova a una data distanza dall'inizio della rete.
    """
    return str(ipaddress.IPv4Address(int(rete.network_address) + offset))


def genera_probabilita_nodi(classe_di_nodo, seed=SEME_F):
    """
    Estrae una volta sola la probabilita' di presenza di ciascun nodo,
    uniforme nell'intervallo della sua classe; resta fissa per tutte le finestre.
    """
    rng = random.Random(seed)
    probabilita_nodi = []
    for classe in classe_di_nodo:
        p_min, p_max = CONFIG_PERCENTUALI[classe]
        probabilita_nodi.append(rng.uniform(p_min, p_max))
    return probabilita_nodi


def calcola_parametri_giorno(giorno, fascia_oraria, pacchetti_base):
    """
    Numero di pacchetti e pesi dei protocolli per giorno e fascia oraria.
    """
    giorno = giorno.lower()
    fo = fascia_oraria.lower()

    # Moltiplicatore del volume per giorno.
    modificatori_giorno = {
        'giorno1': 1.0,     # nella norma
        'giorno2': 1.2,     # picco infrasettimanale
        'giorno3': 1.1,     # leggermente elevato
        'giorno4': 1.0,     # nella norma
        'giorno5': 0.8,     # ridotto
        'giorno6': 0.15,    # minimo (fine settimana)
        'giorno7': 0.10,    # minimo (fine settimana)
        'giorno8': 1.05,    # nella norma
        'giorno9': 1.3,     # picco massimo
        'giorno10': 0.5     # dimezzato
    }
 
    moltiplicatore = modificatori_giorno.get(giorno, 1.0)
    num_pacchetti = int(pacchetti_base * moltiplicatore)

    # Pesi dei protocolli: ARP e DHCP al mattino, TCP e UDP al pomeriggio,
    # ICMP e manutenzione di notte.
    MATRICE_PESI = {
        'giorno1': {
            'mattina':    {'ARP': 19, 'TCP': 19, 'UDP': 15, 'DHCP': 10, 'Ether': 5, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 5, 'BOOTP': 5, 'mDNS': 4, 'SSDP': 3},
            'pomeriggio': {'TCP': 56, 'UDP': 26, 'ARP': 5, 'ICMP': 3, 'Ether': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'SSDP': 1, 'mDNS': 1},
            'notte':      {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 10, 'UDP': 10},
        },
        'giorno2': {
            'mattina':    {'TCP': 25, 'ARP': 18, 'UDP': 15, 'DHCP': 10, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 5, 'Ether': 4, 'BOOTP': 4, 'SSDP': 2, 'mDNS': 2},
            'pomeriggio': {'TCP': 60, 'UDP': 25, 'ARP': 4, 'IP': 2, 'IPv6': 2, 'ICMP': 2, 'ICMPv6EchoRequest': 2, 'Ether': 1, 'SSDP': 1, 'mDNS': 1},
            'notte':      {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 10, 'UDP': 10},
        },
        'giorno3': {
            'mattina':    {'ARP': 19, 'TCP': 19, 'UDP': 15, 'DHCP': 10, 'Ether': 5, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 5, 'BOOTP': 5, 'mDNS': 4, 'SSDP': 3},
            'pomeriggio': {'TCP': 56, 'UDP': 26, 'ARP': 5, 'ICMP': 3, 'Ether': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'SSDP': 1, 'mDNS': 1},
            'notte':      {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 10, 'UDP': 10},
        },
        'giorno4': {
            'mattina':    {'ARP': 19, 'TCP': 19, 'UDP': 15, 'DHCP': 10, 'Ether': 5, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 5, 'BOOTP': 5, 'mDNS': 4, 'SSDP': 3},
            'pomeriggio': {'TCP': 56, 'UDP': 26, 'ARP': 5, 'ICMP': 3, 'Ether': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'SSDP': 1, 'mDNS': 1},
            'notte':      {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 10, 'UDP': 10},
        },
        'giorno5': {
            'mattina':    {'TCP': 24, 'UDP': 19, 'ARP': 14, 'DHCP': 7, 'Ether': 5, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 5, 'BOOTP': 4, 'mDNS': 4, 'SSDP': 3},
            'pomeriggio': {'TCP': 45, 'UDP': 35, 'ARP': 5, 'ICMP': 3, 'Ether': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'SSDP': 2, 'mDNS': 2},
            'notte':      {'ICMP': 20, 'TCP': 15, 'UDP': 15, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'IP': 10, 'IPv6': 10, 'ARP': 5},
        },
        'giorno6': {
            'mattina':    {'ICMP': 20, 'TCP': 15, 'Ether': 10, 'UDP': 10, 'ICMPv6EchoRequest': 10, 'SSDP': 10, 'mDNS': 10, 'ARP': 5, 'IP': 5, 'IPv6': 5},
            'pomeriggio': {'TCP': 30, 'UDP': 15, 'ICMP': 15, 'ICMPv6EchoRequest': 10, 'Ether': 5, 'ARP': 5, 'IP': 5, 'IPv6': 5, 'SSDP': 5, 'mDNS': 5},
            'notte':      {'ICMP': 30, 'ICMPv6EchoRequest': 20, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 5, 'UDP': 5},
        },
        'giorno7': {
            'mattina':    {'ICMP': 25, 'ICMPv6EchoRequest': 11, 'Ether': 10, 'TCP': 10, 'UDP': 10, 'SSDP': 10, 'mDNS': 10, 'IP': 5, 'IPv6': 5, 'ARP': 4},
            'pomeriggio': {'TCP': 25, 'UDP': 20, 'ICMP': 15, 'ICMPv6EchoRequest': 10, 'Ether': 5, 'ARP': 5, 'IP': 5, 'IPv6': 5, 'SSDP': 5, 'mDNS': 5},
            'notte':      {'ICMP': 25, 'Ether': 15, 'ARP': 15, 'ICMPv6EchoRequest': 15, 'IP': 10, 'IPv6': 10, 'TCP': 5, 'UDP': 5},
        },
        'giorno8': {
            'mattina':    {'ARP': 18, 'DHCP': 15, 'TCP': 15, 'UDP': 12, 'BOOTP': 10, 'Ether': 5, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 4, 'mDNS': 3, 'SSDP': 3},
            'pomeriggio': {'TCP': 50, 'UDP': 28, 'ARP': 6, 'ICMP': 4, 'Ether': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'SSDP': 2, 'mDNS': 2},
            'notte':      {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'Ether': 10, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 10, 'UDP': 10},
        },
        'giorno9': {
            'mattina':    {'TCP': 35, 'UDP': 30, 'ARP': 12, 'DHCP': 8, 'IP': 5, 'IPv6': 5, 'ICMP': 5, 'ICMPv6EchoRequest': 4, 'Ether': 3, 'BOOTP': 3, 'SSDP': 1, 'mDNS': 1},
            'pomeriggio': {'TCP': 35, 'UDP': 65, 'ARP': 3, 'ICMP': 2, 'IP': 2, 'IPv6': 2, 'ICMPv6EchoRequest': 2, 'Ether': 1, 'mDNS': 1},
            'notte':      {'TCP': 25, 'UDP': 20, 'ICMP': 15, 'ICMPv6EchoRequest': 10, 'Ether': 8, 'ARP': 8, 'IP': 7, 'IPv6': 7},
        },
        'giorno10': {
            'mattina':    {'ICMP': 25, 'ICMPv6EchoRequest': 15, 'TCP': 12, 'UDP': 10, 'Ether': 8, 'ARP': 8, 'IP': 6, 'IPv6': 6, 'SSDP': 5, 'mDNS': 5},
            'pomeriggio': {'ICMP': 20, 'TCP': 20, 'UDP': 15, 'ICMPv6EchoRequest': 12, 'ARP': 7, 'Ether': 6, 'IP': 6, 'IPv6': 6, 'SSDP': 4, 'mDNS': 4},
            'notte':      {'ICMP': 30, 'ICMPv6EchoRequest': 20, 'Ether': 12, 'ARP': 10, 'IP': 10, 'IPv6': 10, 'TCP': 4, 'UDP': 4},
        },
    }

    pesi_giorno = MATRICE_PESI.get(giorno, MATRICE_PESI['giorno1'])
    percentuali = pesi_giorno.get(fo, pesi_giorno['mattina'])
    pesi = pesi_da_percentuali(percentuali, LISTA_PROTOCOLLI, contesto=f"{giorno}/{fo}")

    # Riduzione notturna (piu' forte nel fine settimana) e mattine del fine
    # settimana dimezzate.
    if fo == 'notte':
        is_weekend = giorno in ['giorno6', 'giorno7']
        num_pacchetti = int(num_pacchetti * 0.2) if is_weekend else int(num_pacchetti * 0.3)
    elif fo == 'mattina' and giorno in ['giorno6', 'giorno7']:
        num_pacchetti = int(num_pacchetti * 0.5)

    return num_pacchetti, LISTA_PROTOCOLLI, pesi


# Genera il file pcap di una finestra (giorno e fascia).
def genera_pcap_campioni(classe_di_nodo, pacchetti_base, giorno, fascia_oraria, nome_file, probabilita_nodi=None, seme_dataset=SEME_DATASET):
    num_nodi = len(classe_di_nodo)

    # Generatore locale: non dipende dallo stato globale ne' dall'ordine delle finestre.
    rng = random.Random(f"{seme_dataset}|{giorno}|{fascia_oraria}")

    # Inizio della finestra, per le marche temporali.
    indice_giorno = int(''.join(c for c in giorno if c.isdigit())) - 1
    indice_fascia = ORDINE_FASCE.index(fascia_oraria.lower())
    ts_finestra = TS_INIZIO + ((indice_giorno * len(ORDINE_FASCE)) + indice_fascia) * DURATA_FASCIA

    num_pacchetti, LISTA_PROTOCOLLI, pesi = calcola_parametri_giorno(giorno, fascia_oraria, pacchetti_base)

    print(f"Generazione -> {nome_file} ({giorno.upper()} - {fascia_oraria.upper()})")
    print(f"\tTarget: {num_pacchetti} pacchetti simulati...")

    mac_nodi = []
    ip_nodi = []
    ipv6_nodi = []

    # Indirizzi deterministici: MAC unico per nodo, server in 192.168.0.x e client numerati dentro la rete della propria classe. La classe C parte dopo i server per non riusarne gli indirizzi.
    contatori_client = {nome: 0 for nome in RETE_DI_CLASSE}
    contatori_client['CLASSE_C'] = NUM_SERVER + 1

    for i in range(1, num_nodi + 1):
        idx = i - 1

        ottetto_3 = (i // 254) % 256
        ottetto_4 = (i % 254) + 1
        mac_nodi.append(f"00:11:22:33:{ottetto_3:02x}:{ottetto_4:02x}")

        classe = classe_di_nodo[idx]
        if classe == 'SERVER':
            ip_nodi.append(f"192.168.{ottetto_3}.{ottetto_4}")
        else:
            contatori_client[classe] += 1
            ip_nodi.append(ip_da_rete(RETE_DI_CLASSE[classe], contatori_client[classe]))

        ipv6_nodi.append(f"fd00::{i:x}")

    idx_server_totali = list(range(0, NUM_SERVER))
    idx_client_totali = list(range(NUM_SERVER, num_nodi))

    # Senza probabilita' tutti i nodi sono presenti.
    if probabilita_nodi is None:
        probabilita_nodi = [1.0] * num_nodi

    # Presenza nella finestra: estrazione indipendente per ciascun nodo.
    idx_server = [i for i in idx_server_totali if rng.random() < probabilita_nodi[i]]
    idx_client = [i for i in idx_client_totali if rng.random() < probabilita_nodi[i]]

    # Nessun nodo viene forzato a essere presente, cosi' le presenze restano indipendenti fra individui. Se manca una delle due categorie, il traffico si compone fra nodi dell'altra.
    pesi_client = [1.0] * len(idx_client)
    pesi_server = [1.0] * len(idx_server)

    lista_pacchetti = []
    passo_ts = DURATA_FASCIA / float(max(num_pacchetti, 1))

    for i in range(num_pacchetti):
        proto = rng.choices(LISTA_PROTOCOLLI, weights=pesi, k=1)[0].upper()

        if idx_server and idx_client:
            # Un client e un server, con verso estratto a caso.
            if rng.random() < 0.5:
                idx_src = rng.choices(idx_client, weights=pesi_client, k=1)[0]
                idx_dst = rng.choices(idx_server, weights=pesi_server, k=1)[0]
            else:
                idx_src = rng.choices(idx_server, weights=pesi_server, k=1)[0]
                idx_dst = rng.choices(idx_client, weights=pesi_client, k=1)[0]
        else:
            # Manca una categoria: due nodi distinti di quella presente.
            presenti = idx_server or idx_client
            if len(presenti) < 2:
                break   # meno di due nodi presenti: nessun pacchetto
            idx_src, idx_dst = rng.sample(presenti, 2)

        eth_src = mac_nodi[idx_src]
        eth_dst = mac_nodi[idx_dst]
        packet = Ether(src=eth_src, dst=eth_dst)

        # Non tutti i pacchetti producono catture nell'analisi: Ether non ha
        # indirizzo di rete, BOOTP e DHCP vanno da 0.0.0.0 al broadcast, SSDP e
        # mDNS hanno destinazione multicast (conta solo il mittente).
        if proto == 'ETHER':
            packet = packet / b"Grezzo livello 2"
        elif proto == 'ARP':
            packet = packet / ARP(hwsrc=eth_src, psrc=ip_nodi[idx_src], pdst=ip_nodi[idx_dst], op=1)
        elif proto == 'IP':
            packet = packet / IP(src=ip_nodi[idx_src], dst=ip_nodi[idx_dst]) / b"Payload IPv4"
        elif proto == 'IPV6':
            packet = packet / IPv6(src=ipv6_nodi[idx_src], dst=ipv6_nodi[idx_dst]) / b"Payload IPv6"
        elif proto == 'TCP':
            dport = 80 if idx_dst in idx_server else rng.randint(1024, 65535)
            packet = packet / IP(src=ip_nodi[idx_src], dst=ip_nodi[idx_dst]) / TCP(sport=rng.randint(1024, 65535), dport=dport, flags="S")
        elif proto == 'UDP':
            dport = 53 if idx_dst in idx_server else rng.randint(1024, 65535)
            packet = packet / IP(src=ip_nodi[idx_src], dst=ip_nodi[idx_dst]) / UDP(sport=rng.randint(1024, 65535), dport=dport)
        elif proto == 'ICMP': 
            packet = packet / IP(src=ip_nodi[idx_src], dst=ip_nodi[idx_dst]) / ICMP()
        elif proto == 'ICMPV6ECHOREQUEST': 
            packet = packet / IPv6(src=ipv6_nodi[idx_src], dst=ipv6_nodi[idx_dst]) / ICMPv6EchoRequest()
        elif proto == 'BOOTP': 
            packet = packet / IP(src="0.0.0.0", dst="255.255.255.255") / UDP(sport=68, dport=67) / BOOTP(chaddr=bytes.fromhex(eth_src.replace(":", "")))
        elif proto == 'DHCP':
            packet.dst = "ff:ff:ff:ff:ff:ff"
            packet = packet / IP(src="0.0.0.0", dst="255.255.255.255") / UDP(sport=68, dport=67) / BOOTP(chaddr=bytes.fromhex(eth_src.replace(":", ""))) / DHCP(options=[("message-type", "discover"), "end"])
        elif proto == 'SSDP':
            packet.dst = "01:00:5e:7f:ff:fa"
            packet = packet / IP(src=ip_nodi[idx_src], dst="239.255.255.250") / UDP(sport=1900, dport=1900) / b"M-SEARCH * HTTP/1.1\r\n"
        elif proto == 'MDNS':
            packet.dst = "01:00:5e:00:00:fb"
            packet = packet / IP(src=ip_nodi[idx_src], dst="224.0.0.251") / UDP(sport=5353, dport=5353) / b"\x00\x00\x00\x00\x00\x01"

        # Pacchetti distribuiti uniformemente nella finestra.
        packet.time = ts_finestra + (i * passo_ts)
        lista_pacchetti.append(packet)

    wrpcap(nome_file, lista_pacchetti)


if __name__ == "__main__":
    PACCHETTI_BASE = 100000

    giorni = ['giorno1', 'giorno2', 'giorno3', 'giorno4', 'giorno5', 'giorno6', 'giorno7', 'giorno8', 'giorno9', 'giorno10']
    fasce = ['mattina', 'pomeriggio', 'notte']

    classe_di_nodo = costruisci_assegnazione_nodi(NUM_SERVER, NODI_PER_CLASSE)
    distribuzione_probabilita = genera_probabilita_nodi(classe_di_nodo)

    totale_nodi = len(classe_di_nodo)
    print("===== INIZIO GENERAZIONE DATASET =====")
    print(f"Profilo benchmark selezionato: '{PROFILO_SELEZIONATO.upper()}'")
    print(f"Semi: F = {SEME_F}, dataset = {SEME_DATASET}")
    print(f"Nodi totali: {totale_nodi}  (SERVER: {NUM_SERVER}, "+ ", ".join(f"{c}: {NODI_PER_CLASSE.get(c, 0)}" for c in ORDINE_CLASSI) + ")")

    for giorno in giorni:
        for fascia in fasce:
            nome_file = f"{giorno}_{fascia}.pcap"
            genera_pcap_campioni(classe_di_nodo, PACCHETTI_BASE, giorno, fascia, nome_file, distribuzione_probabilita, SEME_DATASET)

    print("===== DATASET COMPLETATO: FILE PCAP GENERATI =====")