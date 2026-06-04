#ifndef UNHCOMPRESS_R
#define UNHCOMPRESS_R

#include <stdio.h>

//uncompress_stream:
//  Effectue la décompression complète d'un flux d'entrée vers un flux de
//  sortie. Lit d'abord l'arbre de Huffman sérialisé en notation de
//  Lukasiewicz en tête du flux, le reconstruit en mémoire, puis décode les
//  données bit à bit jusqu'au marqueur de fin de fichier (feuille XLEAF /
//  CASPER).
//  Renvoie 0 en cas de succès, -1 en cas d'erreur (arbre malformé, EOF
//  prématuré dans les données, ou erreur d'écriture sur out).
int uncompress_stream(FILE *in, FILE *out);

#endif
