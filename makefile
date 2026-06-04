.PHONY: all clean dist test

# Compile les deux exécutables
all:
	$(MAKE) -C hcompress_test
	$(MAKE) -C unhcompress_test

# Lance la suite de tests (round-trip + compatibilité si binaires présents)
test: all
	bash tests/test.sh

# Nettoie les objets et exécutables des deux sous-dossiers
clean:
	$(MAKE) -C hcompress_test clean
	$(MAKE) -C unhcompress_test clean

# Produit l'archive de rendu
dist: clean
	tar -hzcf "$(CURDIR).tar.gz" hcompress_test/* unhcompress_test/* huffman/* makefile
