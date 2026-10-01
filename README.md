# tesi-laurea-victorino_informatica-triennale-unipi_2025-26

Repository contenente i codici, i dati e i grafici della tesi di laurea triennale in Informatica, Università di Pisa, a.a. 2025/26.

## Struttura della repository

```
├── network_benchmark_generator.py   generatore della rete simulata e del traffico (pcap)
├── capture_recapture.c              lettura dei pcap e matrice di cattura
├── estimators.c, estimators.h       stimatori Jackknife, Chao e Huggins
├── makefile                         compilazione e target dei 36 test
├── Benchmark_grafici.xlsx           risultati numerici di tutte le configurazioni
├── fig/                             grafici riassuntivi e schema dell'architettura
├── fig3d/                           spettri delle frequenze, un'immagine per popolazione
└── grafici3d_singoli/               spettri delle frequenze, un'immagine per configurazione
```

I file `pcap` non sono inclusi nella repository: si rigenerano in modo identico con il generatore e i semi indicati più avanti.

# Introduzione

Le tecniche di discovery di rete si dividono in attive e passive. Quelle attive interrogano direttamente i dispositivi e ottengono informazioni dettagliate, ma aumentano il traffico e possono disturbare sistemi critici, come gli ICS, o far scattare gli IDS. Quelle passive si limitano ad ascoltare il traffico e non sono intrusive, ma non vedono mai l'intera rete: i dispositivi poco attivi sfuggono all'osservazione, e il numero di nodi osservati resta sempre inferiore a quello reale, di una quantità che non si conosce.

Il progetto applica a questo problema le tecniche di capture-recapture, nate in ecologia per stimare popolazioni animali difficili da contare e poi usate anche in epidemiologia e nell'ingegneria del software. L'idea è osservare la rete in più finestre successive e ricavare, dal modo in cui gli stessi nodi vengono rivisti, una stima di quanti nodi non sono mai stati osservati.

Sono stati implementati tre stimatori per popolazioni chiuse con probabilità di cattura eterogenee: Jackknife (Burnham & Overton, 1978, 1979), Chao (Chao, 1984, 1987) e Huggins (Huggins, 1989, 1991). Il lavoro si inserisce nella ricerca su strumenti a supporto della discovery passiva, come la piattaforma NotLine (Baiardi, Sammartino, Ruggieri, 2025).

# Piano di Lavoro

Il lavoro si è articolato in quattro fasi.

1. **Studio teorico.** Analisi della letteratura sulla capture-recapture e dei modelli per popolazioni chiuse, con particolare attenzione agli stimatori che ammettono eterogeneità fra individui, e definizione della corrispondenza con il dominio di rete: l'indirizzo MAC funge da marca dell'individuo e la finestra di osservazione da occasione di cattura.
2. **Banco di prova.** Sviluppo di un generatore che simula reti con numerosità nota e produce il loro traffico sintetico, così da poter confrontare ogni stima con il valore reale.
3. **Programma di analisi.** Sviluppo di un modulo in C che legge i file di cattura, ricostruisce quali nodi sono stati visti in ciascuna finestra e calcola i tre stimatori con i relativi intervalli di fiducia.
4. **Sperimentazione.** Simulazione di 36 configurazioni (1000, 500 e 100 host; 3, 10, 15 e 30 server; tre profili di traffico), seguite giorno per giorno lungo 10 giornate di osservazione, e confronto dei risultati.

**Risultati principali.** Al termine delle 10 giornate i tre stimatori si discostano in media dalla numerosità reale del 6,9% (Jackknife), del 7,9% (Chao) e del 12,3% (Huggins). Jackknife e Chao risultano complementari nel tempo: Chao è il più accurato nelle prime giornate, Jackknife dalla terza in poi. Usati insieme delimitano un intervallo che contiene la numerosità reale in 35 configurazioni su 36 a fine rilevazione. Il fattore che incide di più sulle stime è la frequenza con cui i nodi si fanno vedere: quando la probabilità media di osservazione scende sotto 0,10, Chao e Huggins sbagliano in media di oltre il 10%.

Fra gli sviluppi futuri c'è la validazione su traffico reale, su un segmento di rete di cui sia noto l'inventario, per misurare l'effetto delle condizioni che nella simulazione sono soddisfatte per costruzione.

# Architettura del sistema

Il sistema è composto da due parti indipendenti, che comunicano soltanto attraverso i file `pcap`.

![Architettura del sistema](fig/architettura_sistema.jpg)

| File | Ruolo |
|------|-------|
| `network_benchmark_generator.py` | Genera una rete simulata e il suo traffico sintetico. Conosce la numerosità reale N e le probabilità di presenza dei nodi, e produce un file `pcap` per ciascuna finestra di osservazione. |
| `capture_recapture.c` | Legge i file `pcap` nell'ordine dato, uno per occasione di cattura, e ricostruisce la storia di cattura di ogni nodo. Non riceve alcuna informazione sul generatore. |
| `estimators.c`, `estimators.h` | Calcolano e stampano gli stimatori a partire dalla matrice di cattura. |
| `makefile` | Compilazione e lancio dei 36 test. |

## Generatore di traffico

La popolazione è composta da un gruppo di server e da quattro classi di client (A, B, C e link-local), ripartiti per il 30% in ciascuna delle classi A, B e C e per il 10% nella classe link-local. A ogni nodo viene assegnata una sola volta una probabilità di presenza, estratta in modo uniforme da un intervallo che dipende dalla sua classe e dal profilo di traffico scelto (`molto_frequente`, `bilanciato`, `poco_frequente`). In ogni finestra la presenza di ciascun nodo è decisa da un'estrazione indipendente, e i pacchetti vengono composti fra i nodi presenti.

Ogni esecuzione produce trenta file, `giorno1_mattina.pcap` ... `giorno10_notte.pcap`. Il volume e la composizione dei protocolli variano per giorno e per fascia oraria.

La generazione è deterministica: con gli stessi semi `SEME_F` e `SEME_DATASET` i file prodotti sono identici byte per byte. Cambiando il solo `SEME_DATASET` si ottiene una replica indipendente della stessa popolazione.

## Programma di analisi

Ogni file `pcap` passato da riga di comando costituisce un'occasione di cattura, nell'ordine in cui compare. Per ogni pacchetto il programma registra separatamente mittente e destinatario: il MAC identifica il nodo, l'indirizzo IP decide se il nodo appartiene alla popolazione osservata. Sono ammessi solo indirizzi privati e link-local (IPv4) e link-local e unique-local (IPv6); un nodo viene contato al più una volta per occasione.

L'output contiene, nell'ordine:

1. la probabilità di cattura per sottorete (diagnostica descrittiva);
2. lo stimatore Jackknife, con tutti gli ordini fino al quinto, il test di selezione dell'ordine e lo stimatore interpolato;
3. lo stimatore di Chao, con i raffinamenti basati sui momenti e gli intervalli simmetrico e log-trasformato;
4. lo stimatore di Huggins, con i sette modelli candidati, la scelta per AIC, i test fra modelli annidati e il bootstrap condizionale.

# Installazione

Dipendenze (Debian/Ubuntu):

```bash
sudo apt install gcc make libpcap-dev libcap-dev python3 python3-pip
pip install scapy
```

Compilazione:

```bash
make            # produce capture_recapture.out
make clean      # rimuove oggetti ed eseguibile
```

Lo stimatore di Huggins ha alcuni parametri di compilazione, modificabili senza toccare il codice:

| Macro | Default | Significato |
|-------|---------|-------------|
| `HUG_USA_COVARIATE` | `1` | Usa la sottorete come covariata individuale (`0` la disattiva). |
| `HUG_MAX_CATEGORIE` | `12` | Numero massimo di categorie della covariata. |
| `HUG_MIN_NODI_CATEGORIA` | `2` | Nodi minimi perché una sottorete abbia una categoria propria. |
| `HUG_BOOT_REPLICHE` | `100` | Repliche del bootstrap condizionale. |

```bash
make clean && make CFLAGS="-Wall -O3 -pthread -DHUG_BOOT_REPLICHE=200"
```

# Esecuzione

## Generare un dataset

I parametri si impostano in testa a `network_benchmark_generator.py`:

| Parametro | Valori usati negli esperimenti |
|-----------|-------------------------------|
| `NUM_SERVER` | 3, 10, 15, 30 |
| `NODI_PER_CLASSE` | A, B, C = 300 e link-local = 100 (1000 host); 150/50 (500 host); 30/10 (100 host) |
| `PROFILO_SELEZIONATO` | `molto_frequente`, `bilanciato`, `poco_frequente` |
| `SEME_F`, `SEME_DATASET` | 1000, 0 |

Il generatore scrive i file nella directory corrente, che vanno poi spostati nella cartella del test corrispondente:

```bash
python3 network_benchmark_generator.py
mkdir -p bftest1 && mv giorno*_*.pcap bftest1/
```

## Lanciare un test

Il programma richiede i privilegi di root. Ogni target `bftestN` del makefile analizza i trenta file della cartella `bftestN/`, cioè l'intera rilevazione (t = 30):

```bash
make bftest1
```

I 36 test corrispondono alle configurazioni seguenti. In ogni terna l'ordine dei profili è `molto_frequente`, `bilanciato`, `poco_frequente`.

| Server | 1000 host | 500 host | 100 host |
|--------|-----------|----------|----------|
| 3      | 1, 2, 3   | 4, 5, 6  | 7, 8, 9  |
| 10     | 10, 11, 12 | 13, 14, 15 | 16, 17, 18 |
| 15     | 19, 20, 21 | 22, 23, 24 | 25, 26, 27 |
| 30     | 28, 29, 30 | 31, 32, 33 | 34, 35, 36 |

Per lanciarli tutti, salvando l'output di ciascuno:

```bash
make
mkdir -p risultati
for n in $(seq 1 36); do
    make -s bftest$n > risultati/bftest$n.txt
done
```

## Uso su catture reali

Il programma accetta qualunque sequenza di file `pcap` con intestazione Ethernet, uno per occasione:

```bash
sudo ./capture_recapture.out cattura1.pcap cattura2.pcap cattura3.pcap ...
```

Jackknife e Chao sono studiati per almeno cinque occasioni: con meno file le stime vengono calcolate ma segnalate come indicative.

# Dati e grafici

## Benchmark_grafici.xlsx

Il file raccoglie l'output del programma di analisi per tutte le configurazioni, giornata per giornata (analisi cumulativa: la giornata d usa le prime 3·d occasioni).

**Fogli `Data Set 1` ... `Data Set 4`.** Ogni foglio corrisponde a un numero di server: 3, 10, 15 e 30. Contiene tre blocchi, uno per taglia della popolazione, nell'ordine 1000, 500 e 100 host. Ogni blocco ha trenta righe, cioè i tre profili di traffico per dieci giornate. Le colonne sono:

| Colonna | Contenuto |
|---------|-----------|
| `profilo_benchmark` | Profilo di traffico (`molto_frequente`, `bilanciato`, `poco_frequente`). |
| `giorno_cattura`, `t-esima cattura` | Giornata (1–10) e numero di occasioni corrispondenti (3–30). |
| `S_obs` | Nodi distinti osservati fino a quella giornata. |
| `f1` ... `f5`, `f_più_alte` | Nodi osservati esattamente 1–5 volte e più di 5 volte. |
| `stima_jackknife`, `stima_chao`, `stima_huggins` | Stime della numerosità dei tre stimatori; per Huggins è quella del modello con AIC minimo. |
| `conf_jackknife_lower/higher` | Intervallo di fiducia al 95% di Jackknife, troncato agli osservati. |
| `conf_chao_lower/upper` | Intervallo simmetrico di Chao, troncato agli osservati. |
| `conf_chao_log_lower/upper` | Intervallo log-trasformato di Chao. |
| `conf_huggins_lower/upper` | Intervallo normale asintotico di Huggins, troncato agli osservati. |
| `popolazione_reale` | Numerosità reale N della configurazione. |

Le celle delle stime sono colorate in base all'errore relativo rispetto a `popolazione_reale`, secondo la legenda in fondo a ciascun foglio: entro il 5%, fra il 5% e il 10%, fra il 10% e il 20%, oltre il 20%.

**Fogli con i grafici.** Ognuno contiene dodici grafici, uno per configurazione:

- `Curve di Accumulazione Catture`: crescita di `S_obs` nelle dieci giornate rispetto alla numerosità reale;
- `Traffico - Molto Frequente`, `Traffico - Bilanciato`, `Traffico - Poco Frequente`: traiettorie delle tre stime per il profilo indicato.

## fig/

Grafici riassuntivi sulle 36 configurazioni, riportati nei capitoli 4 e 5 della tesi.

| File | Contenuto |
|------|-----------|
| `architettura_sistema.jpg` | Schema dell'architettura: generatore, file pcap, programma di analisi e stimatori. |
| `fig1_errore_relativo.png` | Errore relativo medio dei tre stimatori per giornata, separato per profilo di traffico. Per Huggins sono escluse le stime superiori a 3N. |
| `fig2_errore_assoluto.png` | Errore relativo assoluto medio dei tre stimatori per giornata; mostra l'inversione fra Chao e Jackknife alla terza giornata. |
| `fig3_traiettorie_3srv.png` | Traiettorie delle stime nelle configurazioni con 3 server: righe per taglia, colonne per profilo, con N, gli osservati e le bande di confidenza. |
| `fig4_diagnostica.png` | Copertura campionaria (osservati su N) e rapporto fra nodi visti una e due volte, come mediana per profilo. |
| `fig5_intorno.png` | Intorno fra l'estremo inferiore log-trasformato di Chao e l'estremo superiore di Jackknife, per ogni configurazione a fine rilevazione, come scarto percentuale da N. |
| `fig6_accumulazione.png` | Curve di accumulazione degli osservati per tutte le configurazioni, con la numerosità reale come riferimento. |
| `fig7_traiettorie_<profilo>.png` | Traiettorie delle stime per tutte le configurazioni di un profilo, con le stesse convenzioni di `fig3`. |

## fig3d/ e grafici3d_singoli/

Spettri delle frequenze di cattura: superfici che mostrano, per ogni giornata, quanti nodi sono stati osservati esattamente k volte (k da 1 a 5, più la classe aggregata oltre 5).

- `fig3d/DataSet<n>_pop<N>_superficie.png`: un'immagine per popolazione, con i tre profili affiancati sulla stessa scala. `n` è il foglio di `Benchmark_grafici.xlsx` (numero di server) e `N` la numerosità reale.
- `grafici3d_singoli/DataSet<n>_<host>nodi-<server>server/`: le stesse superfici, un file per profilo, a risoluzione maggiore.

L'asse verticale è un numero di nodi, quindi le figure di taglie diverse vanno confrontate nella forma, non nell'altezza. La classe oltre 5 raccoglie venticinque classi e non è confrontabile con le singole, e la superficie interpola fra valori discreti.

# Materiale

## Stimatori implementati

- Otis, D. L., Burnham, K. P., White, G. C., Anderson, D. R. (1978). **Statistical Inference from Capture Data on Closed Animal Populations.** *Wildlife Monographs*, 62, 1–135.
- Burnham, K. P., Overton, W. S. (1978). **Estimation of the Size of a Closed Population when Capture Probabilities Vary Among Animals.** *Biometrika*, 65(3), 625–633.
- Burnham, K. P., Overton, W. S. (1979). **Robust Estimation of Population Size When Capture Probabilities Vary Among Animals.** *Ecology*, 60(5), 927–936.
- Chao, A. (1984). **Nonparametric Estimation of the Number of Classes in a Population.** *Scandinavian Journal of Statistics*, 11(4), 265–270.
- Chao, A. (1987). **Estimating the Population Size for Capture-Recapture Data with Unequal Catchability.** *Biometrics*, 43(4), 783–791.
- Chao, A. (2001). **An Overview of Closed Capture-Recapture Models.** *Journal of Agricultural, Biological, and Environmental Statistics*, 6(2), 158–175.
- Huggins, R. M. (1989). **On the Statistical Analysis of Capture Experiments.** *Biometrika*, 76(1), 133–140.
- Huggins, R. M. (1991). **Some Practical Aspects of a Conditional Likelihood Approach to Capture Experiments.** *Biometrics*, 47(2), 725–732.

## Contesto: discovery passiva

- Baiardi, F., Sammartino, V., Ruggieri, S. (2025). **NotLine: A Non-Intrusive Automated Platform to Build a Digital Twin.** *2025 29th International Symposium on Distributed Simulation and Real Time Applications (DS-RT)*, IEEE, 1–8. doi:[10.1109/DS-RT68115.2025.11185873](https://doi.org/10.1109/DS-RT68115.2025.11185873)

## Applicazioni in ambito ecologico

- Edwards, W. R., Eberhardt, L. L. (1967). **Estimating Cottontail Abundance from Live-Trapping Data.** *Journal of Wildlife Management*, 31(1), 87–96.
- Carothers, A. D. (1973). **Capture-Recapture Methods Applied to a Population with Known Parameters.** *Journal of Animal Ecology*, 42(1), 125–146.
- Pollock, K. H. (1982). **A Capture-Recapture Design Robust to Unequal Probability of Capture.** *Journal of Wildlife Management*, 46(3), 752–757.
- Chao, A., Lee, S.-M. (1992). **Estimating the Number of Classes via Sample Coverage.** *Journal of the American Statistical Association*, 87(417), 210–217.
- Colwell, R. K., Coddington, J. A. (1994). **Estimating Terrestrial Biodiversity Through Extrapolation.** *Philosophical Transactions of the Royal Society of London, Series B*, 345, 101–118.
- Wilson, K. R., Anderson, D. R. (1995). **Continuous-Time Capture-Recapture Population Estimation When Capture Probability Varies Over Time.** *Environmental and Ecological Statistics*, 2, 55–69.
- Alexander, H. M., Slade, N. A., Kettle, W. D. (1997). **Application of Mark-Recapture Models to Estimation of the Population Size of Plants.** *Ecology*, 78, 1230–1237.
- Boulinier, T., Nichols, J. D., Sauer, J. R., Hines, J. E., Pollock, K. H. (1998). **Estimating Species Richness: The Importance of Heterogeneity in Species Detectability.** *Ecology*, 79, 1018–1028.
- Chiu, C.-H., Wang, Y.-T., Walther, B. A., Chao, A. (2014). **An Improved Nonparametric Lower Bound of Species Richness via a Modified Good–Turing Frequency Formula.** *Biometrics*, 70(3), 671–682.
- Chao, A., Chiu, C.-H. (2016). **Species Richness: Estimation and Comparison.** *Wiley StatsRef: Statistics Reference Online*, John Wiley & Sons, 1–26. doi:[10.1002/9781118445112.stat03432.pub2](https://doi.org/10.1002/9781118445112.stat03432.pub2)

## Applicazioni in ambito medico ed epidemiologico

- International Working Group for Disease Monitoring and Forecasting (1995). **Capture-Recapture and Multiple-Record Systems Estimation I: History and Theoretical Development.** *American Journal of Epidemiology*, 142(10), 1047–1058.
- Hook, E. B., Regal, R. R. (1995). **Capture-Recapture Methods in Epidemiology: Methods and Limitations.** *Epidemiologic Reviews*, 17(2), 243–264.
- Chao, A., Tsay, P. K., Shau, W.-Y., Chao, D.-Y. (1996). **Population Size Estimation for Capture-Recapture Models with Applications to Epidemiological Data.** *Proceedings of the American Statistical Association, Biometrics Section*, 108–117.
- Chao, A., Tsay, P. K., Lin, S. H., Shau, W.-Y., Chao, D.-Y. (2001). **The Applications of Capture-Recapture Models to Epidemiological Data.** *Statistics in Medicine*, 20(20), 3123–3157. doi:[10.1002/sim.996](https://doi.org/10.1002/sim.996)
- Crocetti, E., Miccinesi, G., Paci, E., Zappa, M. (2001). **An Application of the Two-Source Capture-Recapture Method to Estimate the Completeness of the Tuscany Cancer Registry, Italy.** *European Journal of Cancer Prevention*, 10(5), 417–423.
- Böhning, D., van der Heijden, P. G. M., Bunge, J. (a cura di) (2017). **Capture-Recapture Methods for the Social and Medical Sciences.** Chapman & Hall/CRC, Boca Raton, FL.

## Applicazioni in ambito informatico

- Nayak, T. K. (1988). **Estimating Population Size by Recapture Sampling.** *Biometrika*, 75(1), 113–120.
- Eick, S. G., Loader, C. R., Long, M. D., Votta, L. G., Vander Wiel, S. A. (1992). **Estimating Software Fault Content Before Coding.** *Proceedings of the 14th International Conference on Software Engineering (ICSE-14)*, 59–65.
- Chao, A., Ma, M. C., Yang, M. C. K. (1993). **Stopping Rules and Estimation for Recapture Debugging with Unequal Failure Rates.** *Biometrika*, 80(1), 193–201.
- Yip, P. (1995). **Estimating the Number of Errors in a System Using a Martingale Approach.** *IEEE Transactions on Reliability*, 44(2), 322–326.
- Voas, J. M., McGraw, G. (1997). **Software Fault Injection: Inoculating Programs Against Errors.** Wiley, New York.
- Briand, L. C., El Emam, K., Freimut, B. G., Laitenberger, O. (2000). **A Comprehensive Evaluation of Capture-Recapture Models for Estimating Software Defect Content.** *IEEE Transactions on Software Engineering*, 26(6), 518–540.

## Riferimenti metodologici

- Harris, B. (1959). **Determining Bounds on Integrals with Applications to Cataloging Problems.** *Annals of Mathematical Statistics*, 30(2), 521–548. (Chao, 1987, riporta erroneamente il volume 20.)
- Pollock, K. H., Hines, J. E., Nichols, J. D. (1984). **The Use of Auxiliary Variables in Capture-Recapture and Removal Experiments.** *Biometrics*, 40(2), 329–340.
- Chao, A., Huggins, R. M. (2005). **Modern Closed-Population Capture-Recapture Models.** In Amstrup, S. C., McDonald, T. L., Manly, B. F. J. (a cura di), *Handbook of Capture-Recapture Analysis*, Princeton University Press, 58–87.