CC=gcc
CFLAGS=-Wall -g -O3 -pthread
LDFLAGS=-pthread
LDLIBS=-lpcap -lcap -lm -lrt

TARGET=capture_recapture
OUTPUT=capture_recapture.out

all: $(TARGET)

capture_recapture: capture_recapture.o estimators.o
	$(CC) $(LDFLAGS) -o $(OUTPUT) $^ $(LDLIBS)

capture_recapture.o: capture_recapture.c estimators.h
	$(CC) $< $(CFLAGS) -c -o $@

estimators.o: estimators.c estimators.h 
	$(CC) $< $(CFLAGS) -c -o $@

# ===== TEST BENCHMARK TRAFFICO SINTETICO =====

bftest1: $(TARGET) # 1000 nodi + 3 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest1/giorno1_mattina.pcap \
		bftest1/giorno1_pomeriggio.pcap \
		bftest1/giorno1_notte.pcap \
		bftest1/giorno2_mattina.pcap \
		bftest1/giorno2_pomeriggio.pcap \
		bftest1/giorno2_notte.pcap \
		bftest1/giorno3_mattina.pcap \
		bftest1/giorno3_pomeriggio.pcap \
		bftest1/giorno3_notte.pcap \
		bftest1/giorno4_mattina.pcap \
		bftest1/giorno4_pomeriggio.pcap \
		bftest1/giorno4_notte.pcap \
		bftest1/giorno5_mattina.pcap \
		bftest1/giorno5_pomeriggio.pcap \
		bftest1/giorno5_notte.pcap \
		bftest1/giorno6_mattina.pcap \
		bftest1/giorno6_pomeriggio.pcap \
		bftest1/giorno6_notte.pcap \
		bftest1/giorno7_mattina.pcap \
		bftest1/giorno7_pomeriggio.pcap \
		bftest1/giorno7_notte.pcap \
		bftest1/giorno8_mattina.pcap \
		bftest1/giorno8_pomeriggio.pcap \
		bftest1/giorno8_notte.pcap \
		bftest1/giorno9_mattina.pcap \
		bftest1/giorno9_pomeriggio.pcap \
		bftest1/giorno9_notte.pcap \
		bftest1/giorno10_mattina.pcap \
		bftest1/giorno10_pomeriggio.pcap \
		bftest1/giorno10_notte.pcap)

bftest2: $(TARGET) # 1000 nodi + 3 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest2/giorno1_mattina.pcap \
		bftest2/giorno1_pomeriggio.pcap \
		bftest2/giorno1_notte.pcap \
		bftest2/giorno2_mattina.pcap \
		bftest2/giorno2_pomeriggio.pcap \
		bftest2/giorno2_notte.pcap \
		bftest2/giorno3_mattina.pcap \
		bftest2/giorno3_pomeriggio.pcap \
		bftest2/giorno3_notte.pcap \
		bftest2/giorno4_mattina.pcap \
		bftest2/giorno4_pomeriggio.pcap \
		bftest2/giorno4_notte.pcap \
		bftest2/giorno5_mattina.pcap \
		bftest2/giorno5_pomeriggio.pcap \
		bftest2/giorno5_notte.pcap \
		bftest2/giorno6_mattina.pcap \
		bftest2/giorno6_pomeriggio.pcap \
		bftest2/giorno6_notte.pcap \
		bftest2/giorno7_mattina.pcap \
		bftest2/giorno7_pomeriggio.pcap \
		bftest2/giorno7_notte.pcap \
		bftest2/giorno8_mattina.pcap \
		bftest2/giorno8_pomeriggio.pcap \
		bftest2/giorno8_notte.pcap \
		bftest2/giorno9_mattina.pcap \
		bftest2/giorno9_pomeriggio.pcap \
		bftest2/giorno9_notte.pcap \
		bftest2/giorno10_mattina.pcap \
		bftest2/giorno10_pomeriggio.pcap \
		bftest2/giorno10_notte.pcap)

bftest3: $(TARGET) # 1000 nodi + 3 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest3/giorno1_mattina.pcap \
		bftest3/giorno1_pomeriggio.pcap \
		bftest3/giorno1_notte.pcap \
		bftest3/giorno2_mattina.pcap \
		bftest3/giorno2_pomeriggio.pcap \
		bftest3/giorno2_notte.pcap \
		bftest3/giorno3_mattina.pcap \
		bftest3/giorno3_pomeriggio.pcap \
		bftest3/giorno3_notte.pcap \
		bftest3/giorno4_mattina.pcap \
		bftest3/giorno4_pomeriggio.pcap \
		bftest3/giorno4_notte.pcap \
		bftest3/giorno5_mattina.pcap \
		bftest3/giorno5_pomeriggio.pcap \
		bftest3/giorno5_notte.pcap \
		bftest3/giorno6_mattina.pcap \
		bftest3/giorno6_pomeriggio.pcap \
		bftest3/giorno6_notte.pcap \
		bftest3/giorno7_mattina.pcap \
		bftest3/giorno7_pomeriggio.pcap \
		bftest3/giorno7_notte.pcap \
		bftest3/giorno8_mattina.pcap \
		bftest3/giorno8_pomeriggio.pcap \
		bftest3/giorno8_notte.pcap \
		bftest3/giorno9_mattina.pcap \
		bftest3/giorno9_pomeriggio.pcap \
		bftest3/giorno9_notte.pcap \
		bftest3/giorno10_mattina.pcap \
		bftest3/giorno10_pomeriggio.pcap \
		bftest3/giorno10_notte.pcap)

bftest4: $(TARGET) # 500 nodi + 3 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest4/giorno1_mattina.pcap \
		bftest4/giorno1_pomeriggio.pcap \
		bftest4/giorno1_notte.pcap \
		bftest4/giorno2_mattina.pcap \
		bftest4/giorno2_pomeriggio.pcap \
		bftest4/giorno2_notte.pcap \
		bftest4/giorno3_mattina.pcap \
		bftest4/giorno3_pomeriggio.pcap \
		bftest4/giorno3_notte.pcap \
		bftest4/giorno4_mattina.pcap \
		bftest4/giorno4_pomeriggio.pcap \
		bftest4/giorno4_notte.pcap \
		bftest4/giorno5_mattina.pcap \
		bftest4/giorno5_pomeriggio.pcap \
		bftest4/giorno5_notte.pcap \
		bftest4/giorno6_mattina.pcap \
		bftest4/giorno6_pomeriggio.pcap \
		bftest4/giorno6_notte.pcap \
		bftest4/giorno7_mattina.pcap \
		bftest4/giorno7_pomeriggio.pcap \
		bftest4/giorno7_notte.pcap \
		bftest4/giorno8_mattina.pcap \
		bftest4/giorno8_pomeriggio.pcap \
		bftest4/giorno8_notte.pcap \
		bftest4/giorno9_mattina.pcap \
		bftest4/giorno9_pomeriggio.pcap \
		bftest4/giorno9_notte.pcap \
		bftest4/giorno10_mattina.pcap \
		bftest4/giorno10_pomeriggio.pcap \
		bftest4/giorno10_notte.pcap)

bftest5: $(TARGET) # 500 nodi + 3 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest5/giorno1_mattina.pcap \
		bftest5/giorno1_pomeriggio.pcap \
		bftest5/giorno1_notte.pcap \
		bftest5/giorno2_mattina.pcap \
		bftest5/giorno2_pomeriggio.pcap \
		bftest5/giorno2_notte.pcap \
		bftest5/giorno3_mattina.pcap \
		bftest5/giorno3_pomeriggio.pcap \
		bftest5/giorno3_notte.pcap \
		bftest5/giorno4_mattina.pcap \
		bftest5/giorno4_pomeriggio.pcap \
		bftest5/giorno4_notte.pcap \
		bftest5/giorno5_mattina.pcap \
		bftest5/giorno5_pomeriggio.pcap \
		bftest5/giorno5_notte.pcap \
		bftest5/giorno6_mattina.pcap \
		bftest5/giorno6_pomeriggio.pcap \
		bftest5/giorno6_notte.pcap \
		bftest5/giorno7_mattina.pcap \
		bftest5/giorno7_pomeriggio.pcap \
		bftest5/giorno7_notte.pcap \
		bftest5/giorno8_mattina.pcap \
		bftest5/giorno8_pomeriggio.pcap \
		bftest5/giorno8_notte.pcap \
		bftest5/giorno9_mattina.pcap \
		bftest5/giorno9_pomeriggio.pcap \
		bftest5/giorno9_notte.pcap \
		bftest5/giorno10_mattina.pcap \
		bftest5/giorno10_pomeriggio.pcap \
		bftest5/giorno10_notte.pcap)

bftest6: $(TARGET) # 500 nodi + 3 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest6/giorno1_mattina.pcap \
		bftest6/giorno1_pomeriggio.pcap \
		bftest6/giorno1_notte.pcap \
		bftest6/giorno2_mattina.pcap \
		bftest6/giorno2_pomeriggio.pcap \
		bftest6/giorno2_notte.pcap \
		bftest6/giorno3_mattina.pcap \
		bftest6/giorno3_pomeriggio.pcap \
		bftest6/giorno3_notte.pcap \
		bftest6/giorno4_mattina.pcap \
		bftest6/giorno4_pomeriggio.pcap \
		bftest6/giorno4_notte.pcap \
		bftest6/giorno5_mattina.pcap \
		bftest6/giorno5_pomeriggio.pcap \
		bftest6/giorno5_notte.pcap \
		bftest6/giorno6_mattina.pcap \
		bftest6/giorno6_pomeriggio.pcap \
		bftest6/giorno6_notte.pcap \
		bftest6/giorno7_mattina.pcap \
		bftest6/giorno7_pomeriggio.pcap \
		bftest6/giorno7_notte.pcap \
		bftest6/giorno8_mattina.pcap \
		bftest6/giorno8_pomeriggio.pcap \
		bftest6/giorno8_notte.pcap \
		bftest6/giorno9_mattina.pcap \
		bftest6/giorno9_pomeriggio.pcap \
		bftest6/giorno9_notte.pcap \
		bftest6/giorno10_mattina.pcap \
		bftest6/giorno10_pomeriggio.pcap \
		bftest6/giorno10_notte.pcap)

bftest7: $(TARGET) # 100 nodi + 3 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest7/giorno1_mattina.pcap \
		bftest7/giorno1_pomeriggio.pcap \
		bftest7/giorno1_notte.pcap \
		bftest7/giorno2_mattina.pcap \
		bftest7/giorno2_pomeriggio.pcap \
		bftest7/giorno2_notte.pcap \
		bftest7/giorno3_mattina.pcap \
		bftest7/giorno3_pomeriggio.pcap \
		bftest7/giorno3_notte.pcap \
		bftest7/giorno4_mattina.pcap \
		bftest7/giorno4_pomeriggio.pcap \
		bftest7/giorno4_notte.pcap \
		bftest7/giorno5_mattina.pcap \
		bftest7/giorno5_pomeriggio.pcap \
		bftest7/giorno5_notte.pcap \
		bftest7/giorno6_mattina.pcap \
		bftest7/giorno6_pomeriggio.pcap \
		bftest7/giorno6_notte.pcap \
		bftest7/giorno7_mattina.pcap \
		bftest7/giorno7_pomeriggio.pcap \
		bftest7/giorno7_notte.pcap \
		bftest7/giorno8_mattina.pcap \
		bftest7/giorno8_pomeriggio.pcap \
		bftest7/giorno8_notte.pcap \
		bftest7/giorno9_mattina.pcap \
		bftest7/giorno9_pomeriggio.pcap \
		bftest7/giorno9_notte.pcap \
		bftest7/giorno10_mattina.pcap \
		bftest7/giorno10_pomeriggio.pcap \
		bftest7/giorno10_notte.pcap)

bftest8: $(TARGET) # 100 nodi + 3 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest8/giorno1_mattina.pcap \
		bftest8/giorno1_pomeriggio.pcap \
		bftest8/giorno1_notte.pcap \
		bftest8/giorno2_mattina.pcap \
		bftest8/giorno2_pomeriggio.pcap \
		bftest8/giorno2_notte.pcap \
		bftest8/giorno3_mattina.pcap \
		bftest8/giorno3_pomeriggio.pcap \
		bftest8/giorno3_notte.pcap \
		bftest8/giorno4_mattina.pcap \
		bftest8/giorno4_pomeriggio.pcap \
		bftest8/giorno4_notte.pcap \
		bftest8/giorno5_mattina.pcap \
		bftest8/giorno5_pomeriggio.pcap \
		bftest8/giorno5_notte.pcap \
		bftest8/giorno6_mattina.pcap \
		bftest8/giorno6_pomeriggio.pcap \
		bftest8/giorno6_notte.pcap \
		bftest8/giorno7_mattina.pcap \
		bftest8/giorno7_pomeriggio.pcap \
		bftest8/giorno7_notte.pcap \
		bftest8/giorno8_mattina.pcap \
		bftest8/giorno8_pomeriggio.pcap \
		bftest8/giorno8_notte.pcap \
		bftest8/giorno9_mattina.pcap \
		bftest8/giorno9_pomeriggio.pcap \
		bftest8/giorno9_notte.pcap \
		bftest8/giorno10_mattina.pcap \
		bftest8/giorno10_pomeriggio.pcap \
		bftest8/giorno10_notte.pcap)

bftest9: $(TARGET) # 100 nodi + 3 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest9/giorno1_mattina.pcap \
		bftest9/giorno1_pomeriggio.pcap \
		bftest9/giorno1_notte.pcap \
		bftest9/giorno2_mattina.pcap \
		bftest9/giorno2_pomeriggio.pcap \
		bftest9/giorno2_notte.pcap \
		bftest9/giorno3_mattina.pcap \
		bftest9/giorno3_pomeriggio.pcap \
		bftest9/giorno3_notte.pcap \
		bftest9/giorno4_mattina.pcap \
		bftest9/giorno4_pomeriggio.pcap \
		bftest9/giorno4_notte.pcap \
		bftest9/giorno5_mattina.pcap \
		bftest9/giorno5_pomeriggio.pcap \
		bftest9/giorno5_notte.pcap \
		bftest9/giorno6_mattina.pcap \
		bftest9/giorno6_pomeriggio.pcap \
		bftest9/giorno6_notte.pcap \
		bftest9/giorno7_mattina.pcap \
		bftest9/giorno7_pomeriggio.pcap \
		bftest9/giorno7_notte.pcap \
		bftest9/giorno8_mattina.pcap \
		bftest9/giorno8_pomeriggio.pcap \
		bftest9/giorno8_notte.pcap \
		bftest9/giorno9_mattina.pcap \
		bftest9/giorno9_pomeriggio.pcap \
		bftest9/giorno9_notte.pcap \
		bftest9/giorno10_mattina.pcap \
		bftest9/giorno10_pomeriggio.pcap \
		bftest9/giorno10_notte.pcap)

bftest10: $(TARGET) # 1000 nodi + 10 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest10/giorno1_mattina.pcap \
		bftest10/giorno1_pomeriggio.pcap \
		bftest10/giorno1_notte.pcap \
		bftest10/giorno2_mattina.pcap \
		bftest10/giorno2_pomeriggio.pcap \
		bftest10/giorno2_notte.pcap \
		bftest10/giorno3_mattina.pcap \
		bftest10/giorno3_pomeriggio.pcap \
		bftest10/giorno3_notte.pcap \
		bftest10/giorno4_mattina.pcap \
		bftest10/giorno4_pomeriggio.pcap \
		bftest10/giorno4_notte.pcap \
		bftest10/giorno5_mattina.pcap \
		bftest10/giorno5_pomeriggio.pcap \
		bftest10/giorno5_notte.pcap \
		bftest10/giorno6_mattina.pcap \
		bftest10/giorno6_pomeriggio.pcap \
		bftest10/giorno6_notte.pcap \
		bftest10/giorno7_mattina.pcap \
		bftest10/giorno7_pomeriggio.pcap \
		bftest10/giorno7_notte.pcap \
		bftest10/giorno8_mattina.pcap \
		bftest10/giorno8_pomeriggio.pcap \
		bftest10/giorno8_notte.pcap \
		bftest10/giorno9_mattina.pcap \
		bftest10/giorno9_pomeriggio.pcap \
		bftest10/giorno9_notte.pcap \
		bftest10/giorno10_mattina.pcap \
		bftest10/giorno10_pomeriggio.pcap \
		bftest10/giorno10_notte.pcap)

bftest11: $(TARGET) # 1000 nodi + 10 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest11/giorno1_mattina.pcap \
		bftest11/giorno1_pomeriggio.pcap \
		bftest11/giorno1_notte.pcap \
		bftest11/giorno2_mattina.pcap \
		bftest11/giorno2_pomeriggio.pcap \
		bftest11/giorno2_notte.pcap \
		bftest11/giorno3_mattina.pcap \
		bftest11/giorno3_pomeriggio.pcap \
		bftest11/giorno3_notte.pcap \
		bftest11/giorno4_mattina.pcap \
		bftest11/giorno4_pomeriggio.pcap \
		bftest11/giorno4_notte.pcap \
		bftest11/giorno5_mattina.pcap \
		bftest11/giorno5_pomeriggio.pcap \
		bftest11/giorno5_notte.pcap \
		bftest11/giorno6_mattina.pcap \
		bftest11/giorno6_pomeriggio.pcap \
		bftest11/giorno6_notte.pcap \
		bftest11/giorno7_mattina.pcap \
		bftest11/giorno7_pomeriggio.pcap \
		bftest11/giorno7_notte.pcap \
		bftest11/giorno8_mattina.pcap \
		bftest11/giorno8_pomeriggio.pcap \
		bftest11/giorno8_notte.pcap \
		bftest11/giorno9_mattina.pcap \
		bftest11/giorno9_pomeriggio.pcap \
		bftest11/giorno9_notte.pcap \
		bftest11/giorno10_mattina.pcap \
		bftest11/giorno10_pomeriggio.pcap \
		bftest11/giorno10_notte.pcap)

bftest12: $(TARGET) # 1000 nodi + 10 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest12/giorno1_mattina.pcap \
		bftest12/giorno1_pomeriggio.pcap \
		bftest12/giorno1_notte.pcap \
		bftest12/giorno2_mattina.pcap \
		bftest12/giorno2_pomeriggio.pcap \
		bftest12/giorno2_notte.pcap \
		bftest12/giorno3_mattina.pcap \
		bftest12/giorno3_pomeriggio.pcap \
		bftest12/giorno3_notte.pcap \
		bftest12/giorno4_mattina.pcap \
		bftest12/giorno4_pomeriggio.pcap \
		bftest12/giorno4_notte.pcap \
		bftest12/giorno5_mattina.pcap \
		bftest12/giorno5_pomeriggio.pcap \
		bftest12/giorno5_notte.pcap \
		bftest12/giorno6_mattina.pcap \
		bftest12/giorno6_pomeriggio.pcap \
		bftest12/giorno6_notte.pcap \
		bftest12/giorno7_mattina.pcap \
		bftest12/giorno7_pomeriggio.pcap \
		bftest12/giorno7_notte.pcap \
		bftest12/giorno8_mattina.pcap \
		bftest12/giorno8_pomeriggio.pcap \
		bftest12/giorno8_notte.pcap \
		bftest12/giorno9_mattina.pcap \
		bftest12/giorno9_pomeriggio.pcap \
		bftest12/giorno9_notte.pcap \
		bftest12/giorno10_mattina.pcap \
		bftest12/giorno10_pomeriggio.pcap \
		bftest12/giorno10_notte.pcap)

bftest13: $(TARGET) # 500 nodi + 10 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest13/giorno1_mattina.pcap \
		bftest13/giorno1_pomeriggio.pcap \
		bftest13/giorno1_notte.pcap \
		bftest13/giorno2_mattina.pcap \
		bftest13/giorno2_pomeriggio.pcap \
		bftest13/giorno2_notte.pcap \
		bftest13/giorno3_mattina.pcap \
		bftest13/giorno3_pomeriggio.pcap \
		bftest13/giorno3_notte.pcap \
		bftest13/giorno4_mattina.pcap \
		bftest13/giorno4_pomeriggio.pcap \
		bftest13/giorno4_notte.pcap \
		bftest13/giorno5_mattina.pcap \
		bftest13/giorno5_pomeriggio.pcap \
		bftest13/giorno5_notte.pcap \
		bftest13/giorno6_mattina.pcap \
		bftest13/giorno6_pomeriggio.pcap \
		bftest13/giorno6_notte.pcap \
		bftest13/giorno7_mattina.pcap \
		bftest13/giorno7_pomeriggio.pcap \
		bftest13/giorno7_notte.pcap \
		bftest13/giorno8_mattina.pcap \
		bftest13/giorno8_pomeriggio.pcap \
		bftest13/giorno8_notte.pcap \
		bftest13/giorno9_mattina.pcap \
		bftest13/giorno9_pomeriggio.pcap \
		bftest13/giorno9_notte.pcap \
		bftest13/giorno10_mattina.pcap \
		bftest13/giorno10_pomeriggio.pcap \
		bftest13/giorno10_notte.pcap)

bftest14: $(TARGET) # 500 nodi + 10 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest14/giorno1_mattina.pcap \
		bftest14/giorno1_pomeriggio.pcap \
		bftest14/giorno1_notte.pcap \
		bftest14/giorno2_mattina.pcap \
		bftest14/giorno2_pomeriggio.pcap \
		bftest14/giorno2_notte.pcap \
		bftest14/giorno3_mattina.pcap \
		bftest14/giorno3_pomeriggio.pcap \
		bftest14/giorno3_notte.pcap \
		bftest14/giorno4_mattina.pcap \
		bftest14/giorno4_pomeriggio.pcap \
		bftest14/giorno4_notte.pcap \
		bftest14/giorno5_mattina.pcap \
		bftest14/giorno5_pomeriggio.pcap \
		bftest14/giorno5_notte.pcap \
		bftest14/giorno6_mattina.pcap \
		bftest14/giorno6_pomeriggio.pcap \
		bftest14/giorno6_notte.pcap \
		bftest14/giorno7_mattina.pcap \
		bftest14/giorno7_pomeriggio.pcap \
		bftest14/giorno7_notte.pcap \
		bftest14/giorno8_mattina.pcap \
		bftest14/giorno8_pomeriggio.pcap \
		bftest14/giorno8_notte.pcap \
		bftest14/giorno9_mattina.pcap \
		bftest14/giorno9_pomeriggio.pcap \
		bftest14/giorno9_notte.pcap \
		bftest14/giorno10_mattina.pcap \
		bftest14/giorno10_pomeriggio.pcap \
		bftest14/giorno10_notte.pcap)

bftest15: $(TARGET) # 500 nodi + 10 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest15/giorno1_mattina.pcap \
		bftest15/giorno1_pomeriggio.pcap \
		bftest15/giorno1_notte.pcap \
		bftest15/giorno2_mattina.pcap \
		bftest15/giorno2_pomeriggio.pcap \
		bftest15/giorno2_notte.pcap \
		bftest15/giorno3_mattina.pcap \
		bftest15/giorno3_pomeriggio.pcap \
		bftest15/giorno3_notte.pcap \
		bftest15/giorno4_mattina.pcap \
		bftest15/giorno4_pomeriggio.pcap \
		bftest15/giorno4_notte.pcap \
		bftest15/giorno5_mattina.pcap \
		bftest15/giorno5_pomeriggio.pcap \
		bftest15/giorno5_notte.pcap \
		bftest15/giorno6_mattina.pcap \
		bftest15/giorno6_pomeriggio.pcap \
		bftest15/giorno6_notte.pcap \
		bftest15/giorno7_mattina.pcap \
		bftest15/giorno7_pomeriggio.pcap \
		bftest15/giorno7_notte.pcap \
		bftest15/giorno8_mattina.pcap \
		bftest15/giorno8_pomeriggio.pcap \
		bftest15/giorno8_notte.pcap \
		bftest15/giorno9_mattina.pcap \
		bftest15/giorno9_pomeriggio.pcap \
		bftest15/giorno9_notte.pcap \
		bftest15/giorno10_mattina.pcap \
		bftest15/giorno10_pomeriggio.pcap \
		bftest15/giorno10_notte.pcap)

bftest16: $(TARGET) # 100 nodi + 10 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest16/giorno1_mattina.pcap \
		bftest16/giorno1_pomeriggio.pcap \
		bftest16/giorno1_notte.pcap \
		bftest16/giorno2_mattina.pcap \
		bftest16/giorno2_pomeriggio.pcap \
		bftest16/giorno2_notte.pcap \
		bftest16/giorno3_mattina.pcap \
		bftest16/giorno3_pomeriggio.pcap \
		bftest16/giorno3_notte.pcap \
		bftest16/giorno4_mattina.pcap \
		bftest16/giorno4_pomeriggio.pcap \
		bftest16/giorno4_notte.pcap \
		bftest16/giorno5_mattina.pcap \
		bftest16/giorno5_pomeriggio.pcap \
		bftest16/giorno5_notte.pcap \
		bftest16/giorno6_mattina.pcap \
		bftest16/giorno6_pomeriggio.pcap \
		bftest16/giorno6_notte.pcap \
		bftest16/giorno7_mattina.pcap \
		bftest16/giorno7_pomeriggio.pcap \
		bftest16/giorno7_notte.pcap \
		bftest16/giorno8_mattina.pcap \
		bftest16/giorno8_pomeriggio.pcap \
		bftest16/giorno8_notte.pcap \
		bftest16/giorno9_mattina.pcap \
		bftest16/giorno9_pomeriggio.pcap \
		bftest16/giorno9_notte.pcap \
		bftest16/giorno10_mattina.pcap \
		bftest16/giorno10_pomeriggio.pcap \
		bftest16/giorno10_notte.pcap)

bftest17: $(TARGET) # 100 nodi + 10 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest17/giorno1_mattina.pcap \
		bftest17/giorno1_pomeriggio.pcap \
		bftest17/giorno1_notte.pcap \
		bftest17/giorno2_mattina.pcap \
		bftest17/giorno2_pomeriggio.pcap \
		bftest17/giorno2_notte.pcap \
		bftest17/giorno3_mattina.pcap \
		bftest17/giorno3_pomeriggio.pcap \
		bftest17/giorno3_notte.pcap \
		bftest17/giorno4_mattina.pcap \
		bftest17/giorno4_pomeriggio.pcap \
		bftest17/giorno4_notte.pcap \
		bftest17/giorno5_mattina.pcap \
		bftest17/giorno5_pomeriggio.pcap \
		bftest17/giorno5_notte.pcap \
		bftest17/giorno6_mattina.pcap \
		bftest17/giorno6_pomeriggio.pcap \
		bftest17/giorno6_notte.pcap \
		bftest17/giorno7_mattina.pcap \
		bftest17/giorno7_pomeriggio.pcap \
		bftest17/giorno7_notte.pcap \
		bftest17/giorno8_mattina.pcap \
		bftest17/giorno8_pomeriggio.pcap \
		bftest17/giorno8_notte.pcap \
		bftest17/giorno9_mattina.pcap \
		bftest17/giorno9_pomeriggio.pcap \
		bftest17/giorno9_notte.pcap \
		bftest17/giorno10_mattina.pcap \
		bftest17/giorno10_pomeriggio.pcap \
		bftest17/giorno10_notte.pcap)

bftest18: $(TARGET) # 100 nodi + 10 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest18/giorno1_mattina.pcap \
		bftest18/giorno1_pomeriggio.pcap \
		bftest18/giorno1_notte.pcap \
		bftest18/giorno2_mattina.pcap \
		bftest18/giorno2_pomeriggio.pcap \
		bftest18/giorno2_notte.pcap \
		bftest18/giorno3_mattina.pcap \
		bftest18/giorno3_pomeriggio.pcap \
		bftest18/giorno3_notte.pcap \
		bftest18/giorno4_mattina.pcap \
		bftest18/giorno4_pomeriggio.pcap \
		bftest18/giorno4_notte.pcap \
		bftest18/giorno5_mattina.pcap \
		bftest18/giorno5_pomeriggio.pcap \
		bftest18/giorno5_notte.pcap \
		bftest18/giorno6_mattina.pcap \
		bftest18/giorno6_pomeriggio.pcap \
		bftest18/giorno6_notte.pcap \
		bftest18/giorno7_mattina.pcap \
		bftest18/giorno7_pomeriggio.pcap \
		bftest18/giorno7_notte.pcap \
		bftest18/giorno8_mattina.pcap \
		bftest18/giorno8_pomeriggio.pcap \
		bftest18/giorno8_notte.pcap \
		bftest18/giorno9_mattina.pcap \
		bftest18/giorno9_pomeriggio.pcap \
		bftest18/giorno9_notte.pcap \
		bftest18/giorno10_mattina.pcap \
		bftest18/giorno10_pomeriggio.pcap \
		bftest18/giorno10_notte.pcap)

bftest19: $(TARGET) # 1000 nodi + 15 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest19/giorno1_mattina.pcap \
		bftest19/giorno1_pomeriggio.pcap \
		bftest19/giorno1_notte.pcap \
		bftest19/giorno2_mattina.pcap \
		bftest19/giorno2_pomeriggio.pcap \
		bftest19/giorno2_notte.pcap \
		bftest19/giorno3_mattina.pcap \
		bftest19/giorno3_pomeriggio.pcap \
		bftest19/giorno3_notte.pcap \
		bftest19/giorno4_mattina.pcap \
		bftest19/giorno4_pomeriggio.pcap \
		bftest19/giorno4_notte.pcap \
		bftest19/giorno5_mattina.pcap \
		bftest19/giorno5_pomeriggio.pcap \
		bftest19/giorno5_notte.pcap \
		bftest19/giorno6_mattina.pcap \
		bftest19/giorno6_pomeriggio.pcap \
		bftest19/giorno6_notte.pcap \
		bftest19/giorno7_mattina.pcap \
		bftest19/giorno7_pomeriggio.pcap \
		bftest19/giorno7_notte.pcap \
		bftest19/giorno8_mattina.pcap \
		bftest19/giorno8_pomeriggio.pcap \
		bftest19/giorno8_notte.pcap \
		bftest19/giorno9_mattina.pcap \
		bftest19/giorno9_pomeriggio.pcap \
		bftest19/giorno9_notte.pcap \
		bftest19/giorno10_mattina.pcap \
		bftest19/giorno10_pomeriggio.pcap \
		bftest19/giorno10_notte.pcap)

bftest20: $(TARGET) # 1000 nodi + 15 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest20/giorno1_mattina.pcap \
		bftest20/giorno1_pomeriggio.pcap \
		bftest20/giorno1_notte.pcap \
		bftest20/giorno2_mattina.pcap \
		bftest20/giorno2_pomeriggio.pcap \
		bftest20/giorno2_notte.pcap \
		bftest20/giorno3_mattina.pcap \
		bftest20/giorno3_pomeriggio.pcap \
		bftest20/giorno3_notte.pcap \
		bftest20/giorno4_mattina.pcap \
		bftest20/giorno4_pomeriggio.pcap \
		bftest20/giorno4_notte.pcap \
		bftest20/giorno5_mattina.pcap \
		bftest20/giorno5_pomeriggio.pcap \
		bftest20/giorno5_notte.pcap \
		bftest20/giorno6_mattina.pcap \
		bftest20/giorno6_pomeriggio.pcap \
		bftest20/giorno6_notte.pcap \
		bftest20/giorno7_mattina.pcap \
		bftest20/giorno7_pomeriggio.pcap \
		bftest20/giorno7_notte.pcap \
		bftest20/giorno8_mattina.pcap \
		bftest20/giorno8_pomeriggio.pcap \
		bftest20/giorno8_notte.pcap \
		bftest20/giorno9_mattina.pcap \
		bftest20/giorno9_pomeriggio.pcap \
		bftest20/giorno9_notte.pcap \
		bftest20/giorno10_mattina.pcap \
		bftest20/giorno10_pomeriggio.pcap \
		bftest20/giorno10_notte.pcap)

bftest21: $(TARGET) # 1000 nodi + 15 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest21/giorno1_mattina.pcap \
		bftest21/giorno1_pomeriggio.pcap \
		bftest21/giorno1_notte.pcap \
		bftest21/giorno2_mattina.pcap \
		bftest21/giorno2_pomeriggio.pcap \
		bftest21/giorno2_notte.pcap \
		bftest21/giorno3_mattina.pcap \
		bftest21/giorno3_pomeriggio.pcap \
		bftest21/giorno3_notte.pcap \
		bftest21/giorno4_mattina.pcap \
		bftest21/giorno4_pomeriggio.pcap \
		bftest21/giorno4_notte.pcap \
		bftest21/giorno5_mattina.pcap \
		bftest21/giorno5_pomeriggio.pcap \
		bftest21/giorno5_notte.pcap \
		bftest21/giorno6_mattina.pcap \
		bftest21/giorno6_pomeriggio.pcap \
		bftest21/giorno6_notte.pcap \
		bftest21/giorno7_mattina.pcap \
		bftest21/giorno7_pomeriggio.pcap \
		bftest21/giorno7_notte.pcap \
		bftest21/giorno8_mattina.pcap \
		bftest21/giorno8_pomeriggio.pcap \
		bftest21/giorno8_notte.pcap \
		bftest21/giorno9_mattina.pcap \
		bftest21/giorno9_pomeriggio.pcap \
		bftest21/giorno9_notte.pcap \
		bftest21/giorno10_mattina.pcap \
		bftest21/giorno10_pomeriggio.pcap \
		bftest21/giorno10_notte.pcap)

bftest22: $(TARGET) # 500 nodi + 15 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest22/giorno1_mattina.pcap \
		bftest22/giorno1_pomeriggio.pcap \
		bftest22/giorno1_notte.pcap \
		bftest22/giorno2_mattina.pcap \
		bftest22/giorno2_pomeriggio.pcap \
		bftest22/giorno2_notte.pcap \
		bftest22/giorno3_mattina.pcap \
		bftest22/giorno3_pomeriggio.pcap \
		bftest22/giorno3_notte.pcap \
		bftest22/giorno4_mattina.pcap \
		bftest22/giorno4_pomeriggio.pcap \
		bftest22/giorno4_notte.pcap \
		bftest22/giorno5_mattina.pcap \
		bftest22/giorno5_pomeriggio.pcap \
		bftest22/giorno5_notte.pcap \
		bftest22/giorno6_mattina.pcap \
		bftest22/giorno6_pomeriggio.pcap \
		bftest22/giorno6_notte.pcap \
		bftest22/giorno7_mattina.pcap \
		bftest22/giorno7_pomeriggio.pcap \
		bftest22/giorno7_notte.pcap \
		bftest22/giorno8_mattina.pcap \
		bftest22/giorno8_pomeriggio.pcap \
		bftest22/giorno8_notte.pcap \
		bftest22/giorno9_mattina.pcap \
		bftest22/giorno9_pomeriggio.pcap \
		bftest22/giorno9_notte.pcap \
		bftest22/giorno10_mattina.pcap \
		bftest22/giorno10_pomeriggio.pcap \
		bftest22/giorno10_notte.pcap)

bftest23: $(TARGET) # 500 nodi + 15 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest23/giorno1_mattina.pcap \
		bftest23/giorno1_pomeriggio.pcap \
		bftest23/giorno1_notte.pcap \
		bftest23/giorno2_mattina.pcap \
		bftest23/giorno2_pomeriggio.pcap \
		bftest23/giorno2_notte.pcap \
		bftest23/giorno3_mattina.pcap \
		bftest23/giorno3_pomeriggio.pcap \
		bftest23/giorno3_notte.pcap \
		bftest23/giorno4_mattina.pcap \
		bftest23/giorno4_pomeriggio.pcap \
		bftest23/giorno4_notte.pcap \
		bftest23/giorno5_mattina.pcap \
		bftest23/giorno5_pomeriggio.pcap \
		bftest23/giorno5_notte.pcap \
		bftest23/giorno6_mattina.pcap \
		bftest23/giorno6_pomeriggio.pcap \
		bftest23/giorno6_notte.pcap \
		bftest23/giorno7_mattina.pcap \
		bftest23/giorno7_pomeriggio.pcap \
		bftest23/giorno7_notte.pcap \
		bftest23/giorno8_mattina.pcap \
		bftest23/giorno8_pomeriggio.pcap \
		bftest23/giorno8_notte.pcap \
		bftest23/giorno9_mattina.pcap \
		bftest23/giorno9_pomeriggio.pcap \
		bftest23/giorno9_notte.pcap \
		bftest23/giorno10_mattina.pcap \
		bftest23/giorno10_pomeriggio.pcap \
		bftest23/giorno10_notte.pcap)

bftest24: $(TARGET) # 500 nodi + 15 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest24/giorno1_mattina.pcap \
		bftest24/giorno1_pomeriggio.pcap \
		bftest24/giorno1_notte.pcap \
		bftest24/giorno2_mattina.pcap \
		bftest24/giorno2_pomeriggio.pcap \
		bftest24/giorno2_notte.pcap \
		bftest24/giorno3_mattina.pcap \
		bftest24/giorno3_pomeriggio.pcap \
		bftest24/giorno3_notte.pcap \
		bftest24/giorno4_mattina.pcap \
		bftest24/giorno4_pomeriggio.pcap \
		bftest24/giorno4_notte.pcap \
		bftest24/giorno5_mattina.pcap \
		bftest24/giorno5_pomeriggio.pcap \
		bftest24/giorno5_notte.pcap \
		bftest24/giorno6_mattina.pcap \
		bftest24/giorno6_pomeriggio.pcap \
		bftest24/giorno6_notte.pcap \
		bftest24/giorno7_mattina.pcap \
		bftest24/giorno7_pomeriggio.pcap \
		bftest24/giorno7_notte.pcap \
		bftest24/giorno8_mattina.pcap \
		bftest24/giorno8_pomeriggio.pcap \
		bftest24/giorno8_notte.pcap \
		bftest24/giorno9_mattina.pcap \
		bftest24/giorno9_pomeriggio.pcap \
		bftest24/giorno9_notte.pcap \
		bftest24/giorno10_mattina.pcap \
		bftest24/giorno10_pomeriggio.pcap \
		bftest24/giorno10_notte.pcap)

bftest25: $(TARGET) # 100 nodi + 15 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest25/giorno1_mattina.pcap \
		bftest25/giorno1_pomeriggio.pcap \
		bftest25/giorno1_notte.pcap \
		bftest25/giorno2_mattina.pcap \
		bftest25/giorno2_pomeriggio.pcap \
		bftest25/giorno2_notte.pcap \
		bftest25/giorno3_mattina.pcap \
		bftest25/giorno3_pomeriggio.pcap \
		bftest25/giorno3_notte.pcap \
		bftest25/giorno4_mattina.pcap \
		bftest25/giorno4_pomeriggio.pcap \
		bftest25/giorno4_notte.pcap \
		bftest25/giorno5_mattina.pcap \
		bftest25/giorno5_pomeriggio.pcap \
		bftest25/giorno5_notte.pcap \
		bftest25/giorno6_mattina.pcap \
		bftest25/giorno6_pomeriggio.pcap \
		bftest25/giorno6_notte.pcap \
		bftest25/giorno7_mattina.pcap \
		bftest25/giorno7_pomeriggio.pcap \
		bftest25/giorno7_notte.pcap \
		bftest25/giorno8_mattina.pcap \
		bftest25/giorno8_pomeriggio.pcap \
		bftest25/giorno8_notte.pcap \
		bftest25/giorno9_mattina.pcap \
		bftest25/giorno9_pomeriggio.pcap \
		bftest25/giorno9_notte.pcap \
		bftest25/giorno10_mattina.pcap \
		bftest25/giorno10_pomeriggio.pcap \
		bftest25/giorno10_notte.pcap)

bftest26: $(TARGET) # 100 nodi + 15 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest26/giorno1_mattina.pcap \
		bftest26/giorno1_pomeriggio.pcap \
		bftest26/giorno1_notte.pcap \
		bftest26/giorno2_mattina.pcap \
		bftest26/giorno2_pomeriggio.pcap \
		bftest26/giorno2_notte.pcap \
		bftest26/giorno3_mattina.pcap \
		bftest26/giorno3_pomeriggio.pcap \
		bftest26/giorno3_notte.pcap \
		bftest26/giorno4_mattina.pcap \
		bftest26/giorno4_pomeriggio.pcap \
		bftest26/giorno4_notte.pcap \
		bftest26/giorno5_mattina.pcap \
		bftest26/giorno5_pomeriggio.pcap \
		bftest26/giorno5_notte.pcap \
		bftest26/giorno6_mattina.pcap \
		bftest26/giorno6_pomeriggio.pcap \
		bftest26/giorno6_notte.pcap \
		bftest26/giorno7_mattina.pcap \
		bftest26/giorno7_pomeriggio.pcap \
		bftest26/giorno7_notte.pcap \
		bftest26/giorno8_mattina.pcap \
		bftest26/giorno8_pomeriggio.pcap \
		bftest26/giorno8_notte.pcap \
		bftest26/giorno9_mattina.pcap \
		bftest26/giorno9_pomeriggio.pcap \
		bftest26/giorno9_notte.pcap \
		bftest26/giorno10_mattina.pcap \
		bftest26/giorno10_pomeriggio.pcap \
		bftest26/giorno10_notte.pcap)

bftest27: $(TARGET) # 100 nodi + 15 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest27/giorno1_mattina.pcap \
		bftest27/giorno1_pomeriggio.pcap \
		bftest27/giorno1_notte.pcap \
		bftest27/giorno2_mattina.pcap \
		bftest27/giorno2_pomeriggio.pcap \
		bftest27/giorno2_notte.pcap \
		bftest27/giorno3_mattina.pcap \
		bftest27/giorno3_pomeriggio.pcap \
		bftest27/giorno3_notte.pcap \
		bftest27/giorno4_mattina.pcap \
		bftest27/giorno4_pomeriggio.pcap \
		bftest27/giorno4_notte.pcap \
		bftest27/giorno5_mattina.pcap \
		bftest27/giorno5_pomeriggio.pcap \
		bftest27/giorno5_notte.pcap \
		bftest27/giorno6_mattina.pcap \
		bftest27/giorno6_pomeriggio.pcap \
		bftest27/giorno6_notte.pcap \
		bftest27/giorno7_mattina.pcap \
		bftest27/giorno7_pomeriggio.pcap \
		bftest27/giorno7_notte.pcap \
		bftest27/giorno8_mattina.pcap \
		bftest27/giorno8_pomeriggio.pcap \
		bftest27/giorno8_notte.pcap \
		bftest27/giorno9_mattina.pcap \
		bftest27/giorno9_pomeriggio.pcap \
		bftest27/giorno9_notte.pcap \
		bftest27/giorno10_mattina.pcap \
		bftest27/giorno10_pomeriggio.pcap \
		bftest27/giorno10_notte.pcap)

bftest28: $(TARGET) # 1000 nodi + 30 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest28/giorno1_mattina.pcap \
		bftest28/giorno1_pomeriggio.pcap \
		bftest28/giorno1_notte.pcap \
		bftest28/giorno2_mattina.pcap \
		bftest28/giorno2_pomeriggio.pcap \
		bftest28/giorno2_notte.pcap \
		bftest28/giorno3_mattina.pcap \
		bftest28/giorno3_pomeriggio.pcap \
		bftest28/giorno3_notte.pcap \
		bftest28/giorno4_mattina.pcap \
		bftest28/giorno4_pomeriggio.pcap \
		bftest28/giorno4_notte.pcap \
		bftest28/giorno5_mattina.pcap \
		bftest28/giorno5_pomeriggio.pcap \
		bftest28/giorno5_notte.pcap \
		bftest28/giorno6_mattina.pcap \
		bftest28/giorno6_pomeriggio.pcap \
		bftest28/giorno6_notte.pcap \
		bftest28/giorno7_mattina.pcap \
		bftest28/giorno7_pomeriggio.pcap \
		bftest28/giorno7_notte.pcap \
		bftest28/giorno8_mattina.pcap \
		bftest28/giorno8_pomeriggio.pcap \
		bftest28/giorno8_notte.pcap \
		bftest28/giorno9_mattina.pcap \
		bftest28/giorno9_pomeriggio.pcap \
		bftest28/giorno9_notte.pcap \
		bftest28/giorno10_mattina.pcap \
		bftest28/giorno10_pomeriggio.pcap \
		bftest28/giorno10_notte.pcap)

bftest29: $(TARGET) # 1000 nodi + 30 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest29/giorno1_mattina.pcap \
		bftest29/giorno1_pomeriggio.pcap \
		bftest29/giorno1_notte.pcap \
		bftest29/giorno2_mattina.pcap \
		bftest29/giorno2_pomeriggio.pcap \
		bftest29/giorno2_notte.pcap \
		bftest29/giorno3_mattina.pcap \
		bftest29/giorno3_pomeriggio.pcap \
		bftest29/giorno3_notte.pcap \
		bftest29/giorno4_mattina.pcap \
		bftest29/giorno4_pomeriggio.pcap \
		bftest29/giorno4_notte.pcap \
		bftest29/giorno5_mattina.pcap \
		bftest29/giorno5_pomeriggio.pcap \
		bftest29/giorno5_notte.pcap \
		bftest29/giorno6_mattina.pcap \
		bftest29/giorno6_pomeriggio.pcap \
		bftest29/giorno6_notte.pcap \
		bftest29/giorno7_mattina.pcap \
		bftest29/giorno7_pomeriggio.pcap \
		bftest29/giorno7_notte.pcap \
		bftest29/giorno8_mattina.pcap \
		bftest29/giorno8_pomeriggio.pcap \
		bftest29/giorno8_notte.pcap \
		bftest29/giorno9_mattina.pcap \
		bftest29/giorno9_pomeriggio.pcap \
		bftest29/giorno9_notte.pcap \
		bftest29/giorno10_mattina.pcap \
		bftest29/giorno10_pomeriggio.pcap \
		bftest29/giorno10_notte.pcap)

bftest30: $(TARGET) # 1000 nodi + 30 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest30/giorno1_mattina.pcap \
		bftest30/giorno1_pomeriggio.pcap \
		bftest30/giorno1_notte.pcap \
		bftest30/giorno2_mattina.pcap \
		bftest30/giorno2_pomeriggio.pcap \
		bftest30/giorno2_notte.pcap \
		bftest30/giorno3_mattina.pcap \
		bftest30/giorno3_pomeriggio.pcap \
		bftest30/giorno3_notte.pcap \
		bftest30/giorno4_mattina.pcap \
		bftest30/giorno4_pomeriggio.pcap \
		bftest30/giorno4_notte.pcap \
		bftest30/giorno5_mattina.pcap \
		bftest30/giorno5_pomeriggio.pcap \
		bftest30/giorno5_notte.pcap \
		bftest30/giorno6_mattina.pcap \
		bftest30/giorno6_pomeriggio.pcap \
		bftest30/giorno6_notte.pcap \
		bftest30/giorno7_mattina.pcap \
		bftest30/giorno7_pomeriggio.pcap \
		bftest30/giorno7_notte.pcap \
		bftest30/giorno8_mattina.pcap \
		bftest30/giorno8_pomeriggio.pcap \
		bftest30/giorno8_notte.pcap \
		bftest30/giorno9_mattina.pcap \
		bftest30/giorno9_pomeriggio.pcap \
		bftest30/giorno9_notte.pcap \
		bftest30/giorno10_mattina.pcap \
		bftest30/giorno10_pomeriggio.pcap \
		bftest30/giorno10_notte.pcap)

bftest31: $(TARGET) # 500 nodi + 30 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest31/giorno1_mattina.pcap \
		bftest31/giorno1_pomeriggio.pcap \
		bftest31/giorno1_notte.pcap \
		bftest31/giorno2_mattina.pcap \
		bftest31/giorno2_pomeriggio.pcap \
		bftest31/giorno2_notte.pcap \
		bftest31/giorno3_mattina.pcap \
		bftest31/giorno3_pomeriggio.pcap \
		bftest31/giorno3_notte.pcap \
		bftest31/giorno4_mattina.pcap \
		bftest31/giorno4_pomeriggio.pcap \
		bftest31/giorno4_notte.pcap \
		bftest31/giorno5_mattina.pcap \
		bftest31/giorno5_pomeriggio.pcap \
		bftest31/giorno5_notte.pcap \
		bftest31/giorno6_mattina.pcap \
		bftest31/giorno6_pomeriggio.pcap \
		bftest31/giorno6_notte.pcap \
		bftest31/giorno7_mattina.pcap \
		bftest31/giorno7_pomeriggio.pcap \
		bftest31/giorno7_notte.pcap \
		bftest31/giorno8_mattina.pcap \
		bftest31/giorno8_pomeriggio.pcap \
		bftest31/giorno8_notte.pcap \
		bftest31/giorno9_mattina.pcap \
		bftest31/giorno9_pomeriggio.pcap \
		bftest31/giorno9_notte.pcap \
		bftest31/giorno10_mattina.pcap \
		bftest31/giorno10_pomeriggio.pcap \
		bftest31/giorno10_notte.pcap)

bftest32: $(TARGET) # 500 nodi + 30 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest32/giorno1_mattina.pcap \
		bftest32/giorno1_pomeriggio.pcap \
		bftest32/giorno1_notte.pcap \
		bftest32/giorno2_mattina.pcap \
		bftest32/giorno2_pomeriggio.pcap \
		bftest32/giorno2_notte.pcap \
		bftest32/giorno3_mattina.pcap \
		bftest32/giorno3_pomeriggio.pcap \
		bftest32/giorno3_notte.pcap \
		bftest32/giorno4_mattina.pcap \
		bftest32/giorno4_pomeriggio.pcap \
		bftest32/giorno4_notte.pcap \
		bftest32/giorno5_mattina.pcap \
		bftest32/giorno5_pomeriggio.pcap \
		bftest32/giorno5_notte.pcap \
		bftest32/giorno6_mattina.pcap \
		bftest32/giorno6_pomeriggio.pcap \
		bftest32/giorno6_notte.pcap \
		bftest32/giorno7_mattina.pcap \
		bftest32/giorno7_pomeriggio.pcap \
		bftest32/giorno7_notte.pcap \
		bftest32/giorno8_mattina.pcap \
		bftest32/giorno8_pomeriggio.pcap \
		bftest32/giorno8_notte.pcap \
		bftest32/giorno9_mattina.pcap \
		bftest32/giorno9_pomeriggio.pcap \
		bftest32/giorno9_notte.pcap \
		bftest32/giorno10_mattina.pcap \
		bftest32/giorno10_pomeriggio.pcap \
		bftest32/giorno10_notte.pcap)

bftest33: $(TARGET) # 500 nodi + 30 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest33/giorno1_mattina.pcap \
		bftest33/giorno1_pomeriggio.pcap \
		bftest33/giorno1_notte.pcap \
		bftest33/giorno2_mattina.pcap \
		bftest33/giorno2_pomeriggio.pcap \
		bftest33/giorno2_notte.pcap \
		bftest33/giorno3_mattina.pcap \
		bftest33/giorno3_pomeriggio.pcap \
		bftest33/giorno3_notte.pcap \
		bftest33/giorno4_mattina.pcap \
		bftest33/giorno4_pomeriggio.pcap \
		bftest33/giorno4_notte.pcap \
		bftest33/giorno5_mattina.pcap \
		bftest33/giorno5_pomeriggio.pcap \
		bftest33/giorno5_notte.pcap \
		bftest33/giorno6_mattina.pcap \
		bftest33/giorno6_pomeriggio.pcap \
		bftest33/giorno6_notte.pcap \
		bftest33/giorno7_mattina.pcap \
		bftest33/giorno7_pomeriggio.pcap \
		bftest33/giorno7_notte.pcap \
		bftest33/giorno8_mattina.pcap \
		bftest33/giorno8_pomeriggio.pcap \
		bftest33/giorno8_notte.pcap \
		bftest33/giorno9_mattina.pcap \
		bftest33/giorno9_pomeriggio.pcap \
		bftest33/giorno9_notte.pcap \
		bftest33/giorno10_mattina.pcap \
		bftest33/giorno10_pomeriggio.pcap \
		bftest33/giorno10_notte.pcap)

bftest34: $(TARGET) # 100 nodi + 30 server (molto_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest34/giorno1_mattina.pcap \
		bftest34/giorno1_pomeriggio.pcap \
		bftest34/giorno1_notte.pcap \
		bftest34/giorno2_mattina.pcap \
		bftest34/giorno2_pomeriggio.pcap \
		bftest34/giorno2_notte.pcap \
		bftest34/giorno3_mattina.pcap \
		bftest34/giorno3_pomeriggio.pcap \
		bftest34/giorno3_notte.pcap \
		bftest34/giorno4_mattina.pcap \
		bftest34/giorno4_pomeriggio.pcap \
		bftest34/giorno4_notte.pcap \
		bftest34/giorno5_mattina.pcap \
		bftest34/giorno5_pomeriggio.pcap \
		bftest34/giorno5_notte.pcap \
		bftest34/giorno6_mattina.pcap \
		bftest34/giorno6_pomeriggio.pcap \
		bftest34/giorno6_notte.pcap \
		bftest34/giorno7_mattina.pcap \
		bftest34/giorno7_pomeriggio.pcap \
		bftest34/giorno7_notte.pcap \
		bftest34/giorno8_mattina.pcap \
		bftest34/giorno8_pomeriggio.pcap \
		bftest34/giorno8_notte.pcap \
		bftest34/giorno9_mattina.pcap \
		bftest34/giorno9_pomeriggio.pcap \
		bftest34/giorno9_notte.pcap \
		bftest34/giorno10_mattina.pcap \
		bftest34/giorno10_pomeriggio.pcap \
		bftest34/giorno10_notte.pcap)

bftest35: $(TARGET) # 100 nodi + 30 server (bilanciato)
	sudo ./$(OUTPUT) $(wildcard \
		bftest35/giorno1_mattina.pcap \
		bftest35/giorno1_pomeriggio.pcap \
		bftest35/giorno1_notte.pcap \
		bftest35/giorno2_mattina.pcap \
		bftest35/giorno2_pomeriggio.pcap \
		bftest35/giorno2_notte.pcap \
		bftest35/giorno3_mattina.pcap \
		bftest35/giorno3_pomeriggio.pcap \
		bftest35/giorno3_notte.pcap \
		bftest35/giorno4_mattina.pcap \
		bftest35/giorno4_pomeriggio.pcap \
		bftest35/giorno4_notte.pcap \
		bftest35/giorno5_mattina.pcap \
		bftest35/giorno5_pomeriggio.pcap \
		bftest35/giorno5_notte.pcap \
		bftest35/giorno6_mattina.pcap \
		bftest35/giorno6_pomeriggio.pcap \
		bftest35/giorno6_notte.pcap \
		bftest35/giorno7_mattina.pcap \
		bftest35/giorno7_pomeriggio.pcap \
		bftest35/giorno7_notte.pcap \
		bftest35/giorno8_mattina.pcap \
		bftest35/giorno8_pomeriggio.pcap \
		bftest35/giorno8_notte.pcap \
		bftest35/giorno9_mattina.pcap \
		bftest35/giorno9_pomeriggio.pcap \
		bftest35/giorno9_notte.pcap \
		bftest35/giorno10_mattina.pcap \
		bftest35/giorno10_pomeriggio.pcap \
		bftest35/giorno10_notte.pcap)

bftest36: $(TARGET) # 100 nodi + 30 server (poco_frequenti)
	sudo ./$(OUTPUT) $(wildcard \
		bftest36/giorno1_mattina.pcap \
		bftest36/giorno1_pomeriggio.pcap \
		bftest36/giorno1_notte.pcap \
		bftest36/giorno2_mattina.pcap \
		bftest36/giorno2_pomeriggio.pcap \
		bftest36/giorno2_notte.pcap \
		bftest36/giorno3_mattina.pcap \
		bftest36/giorno3_pomeriggio.pcap \
		bftest36/giorno3_notte.pcap \
		bftest36/giorno4_mattina.pcap \
		bftest36/giorno4_pomeriggio.pcap \
		bftest36/giorno4_notte.pcap \
		bftest36/giorno5_mattina.pcap \
		bftest36/giorno5_pomeriggio.pcap \
		bftest36/giorno5_notte.pcap \
		bftest36/giorno6_mattina.pcap \
		bftest36/giorno6_pomeriggio.pcap \
		bftest36/giorno6_notte.pcap \
		bftest36/giorno7_mattina.pcap \
		bftest36/giorno7_pomeriggio.pcap \
		bftest36/giorno7_notte.pcap \
		bftest36/giorno8_mattina.pcap \
		bftest36/giorno8_pomeriggio.pcap \
		bftest36/giorno8_notte.pcap \
		bftest36/giorno9_mattina.pcap \
		bftest36/giorno9_pomeriggio.pcap \
		bftest36/giorno9_notte.pcap \
		bftest36/giorno10_mattina.pcap \
		bftest36/giorno10_pomeriggio.pcap \
		bftest36/giorno10_notte.pcap)

clean:
	rm -f *.o *.out