#ifndef HCOMPRESS_R
#define HCOMPRESS_R

#include <stdio.h>

// init_table :
//   Initialise l'ensemble de la structure par les valeur par defaut
void init_table();

//stock_in_table :
//  Parcourt le flux d'entrée et incrémente le poids des caractères lus.
//  À la fin, donne un poids de 1 à CASPER
// Suppose le fichier d'entrée stream déjà ouvert
void stock_in_table(FILE *stream);

//compar:
// Fonction de comparaison pour qsort (ordre croissant sur le poids).
//  a Premier élément b Second élément
// renvoie -1, 0 ou 1 selon comparaison des wgt
int compar(const void *a, const void *b);

//skip_zeros:
// Détermine l'indice de la première feuille de poids non nul après tri.
// renvoie l'indice du premier élément avec wgt > 0, ou CASPER+1 si aucun
int skip_zeros();

//node_construction:
//  Construit l'arbre de Huffman à partir des feuilles actives.
//   ileaf Indice de la première feuille de poids non nul
//  renvoie l'indice de la racine de l'arbre
int node_construction(int ileaf);

//generate_code:
//  Génère les codes Huffman pour toutes les feuilles de l'arbre.
//  iroot Indice de la racine de l'arbre
int generate_code(int iroot);

//lukas_tree_rep:
//  Écrit la représentation de l'arbre en notation de Lukasiewicz.
//  Format : '0' pour un nœud interne, '1' pour une feuille suivie
//  de la valeur sur CHAR_BIT+1 bits (poids fort en premier).
//  iroot Indice de la racine,
//  s Chaîne de sortie supposé de taille sufisante
void lukas_tree_rep(int iroot, char *s);

//  compress_stream : renvoie -1 si in est stdin ou si une erreur de lecture,
//    d'écriture ou de dépassement de capacité survient. Compresse sinon le
//    contenu de in vers out au format Huffman : calcule les fréquences des
//    caractères, construit l'arbre de Huffman, génère les codes binaires,
//    écrit les données compressées bit à bit suivies du code de fin de fichier
//    puis de la représentation de Lukasiewicz de l'arbre, complète le dernier
//    octet par des zéros si nécessaire. Renvoie 0 en cas de succès.
int compress_stream(FILE *in, FILE *out);

//free_codes:
// Libère la mémoire allouée pour les codes Huffman
void free_codes(void);

#endif
