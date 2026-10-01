# progettoFinale_TesiDiLaurea_2025-26

Repositery contenente i codici richiesti per il progetto finale di TESI DI LAUREA a.a 2025/26 - UNIPI Corso di Informatica

# Introduzione

Il progetto riguarda l’uso di tecniche di capture-recapture per stimare il numero di nodi non individuati durante una scansione passiva della rete. L’obiettivo è sfruttare osservazioni multiple e complementari per inferire la presenza di nodi nascosti, non direttamente visibili, e ottenere una stima più robusta della dimensione reale dell’infrastruttura. Questo approccio può essere utile in contesti in cui la visibilità della rete è parziale o incompleta, e in cui è necessario valutare con maggiore accuratezza la copertura delle attività di discovery.

# Piano di Lavoro

L’attività di ricerca si aprirà con una fase iniziale dedicata all’approfondimento teorico della piattaforma NotLine  e allo studio dei modelli statistici di capture-recapture, con l’obiettivo di comprendere come adattare i classici algoritmi di stima della popolazione al contesto del monitoraggio di rete. Una volta consolidata la base teorica, il lavoro proseguirà con la progettazione di una strategia di acquisizione dati basata su osservazioni passive multiple, definendo i criteri temporali e spaziali necessari per ottenere campionamenti che siano tra loro complementari e statisticamente significativi.

Successivamente, il cuore del progetto si sposterà sullo sviluppo e sull’integrazione di un modulo software capace di elaborare i dati raccolti, applicando i modelli di inferenza per calcolare la probabilità di presenza di nodi nascosti all'interno dell’infrastruttura cyber-fisica. Questa fase richiederà un’attenzione particolare nella gestione dei falsi positivi e nella corretta categorizzazione delle identità rilevate per garantire la precisione delle stime.

La parte finale del percorso sarà dedicata alla validazione sperimentale del metodo attraverso il gemello digitale messo a disposizione da NotLine. In questo ambiente controllato, sarà possibile simulare diverse configurazioni di rete con un numero noto di nodi "invisibili" per misurare l’accuratezza dell’algoritmo e la sua capacità di risposta in scenari di visibilità degradata. Il lavoro si concluderà con la redazione della tesi, in cui verranno discussi i risultati ottenuti e valutato l'impatto di tale approccio sulla resilienza e sulla sicurezza complessiva del sistema monitorato.

# Materiale

Materiale su capture-recapture: 

[Chao (1984)]: **Nonparametric estimation of the number of classes in a population.**
- Introduce l'estimatore alla base del calcolo dei nodi mancanti.

[Chao (1987)]: **Estimating the population size for capture-recapture data with unequal catchability.**
- Estende il modello ai casi in cui i soggetti (o i device) hanno probabilità diverse di essere catturati.

[Burnham & Overton (1979)]: **Robust estimation of population size when capture probabilities vary among animals.**
- Uno dei lavori fondamentali per la stima di popolazioni "chiuse" con eterogeneità.

