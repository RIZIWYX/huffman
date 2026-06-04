#include "unhcompress.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//Taille maximale du tableau (feuilles + nœuds)
#define SIZE ((UCHAR_MAX + 1) * 2 + 1)

// Indice de la feuille contenant le caractere fantome
#define CASPER (UCHAR_MAX + 1)

typedef struct {
  enum {
    NODE,
    LEAF,
    XLEAF
  } mark;
  union {
    int next[2];
    int val;
  };
} hitem;

typedef struct {
  FILE *f;
  unsigned char buffer;
  int bit_count;
  int eof;
} bit_reader;

hitem table_ascii[SIZE];
static int next_index;

static int read_bit(bit_reader *br) {
  if (br->bit_count == 0) {
    int c = fgetc(br->f);
    if (c == EOF) {
      br->eof = 1;
      return -1;
    }
    br->buffer = (unsigned char) c;
    br->bit_count = 8;
  }
  int bit = (br->buffer >> 7) & 1;
  br->buffer <<= 1;
  br->bit_count--;
  return bit;
}

//  read_tree_rec : reconstruit récursivement l'arbre de Huffman à partir de sa
//    notation de Lukasiewicz lue dans br. Le garde next_index >= SIZE borne le
//    nombre de nœuds : un arbre malformé (ou une entrée hostile) ne peut donc
//    pas provoquer de débordement du tableau ni de récursion non bornée.
//    Renvoie l'indice du nœud créé, ou -1 en cas d'EOF prématuré ou de
//    dépassement de capacité.
static int read_tree_rec(bit_reader *br) {
  if (next_index >= SIZE) {
    return -1;
  }
  int my_index = next_index++;
  int bit = read_bit(br);
  if (bit < 0) {
    return -1;
  }
  if (bit == 1) {
    // feuille : lire CHAR_BIT + 1 = 9 bits
    int val = 0;
    for (int b = 0; b < CHAR_BIT + 1; b++) {
      int x = read_bit(br);
      if (x < 0) {
        return -1;
      }
      val = (val << 1) | x;
    }
    if (val == CASPER) {
      table_ascii[my_index].mark = XLEAF;
    } else {
      table_ascii[my_index].mark = LEAF;
      table_ascii[my_index].val = val;
    }
  } else {
    int left = read_tree_rec(br);
    if (left < 0) {
      return -1;
    }
    int right = read_tree_rec(br);
    if (right < 0) {
      return -1;
    }
    table_ascii[my_index].mark = NODE;
    table_ascii[my_index].next[0] = left;
    table_ascii[my_index].next[1] = right;
  }
  return my_index;
}

//  decompress_data : décode le flux de bits restant dans br en partant de la
//    racine iroot. Descend à gauche sur un bit 0, à droite sur un bit 1, émet
//    le caractère de chaque feuille atteinte sur out, et s'arrête à la feuille
//    XLEAF (marqueur de fin). Renvoie 0 en cas de succès, -1 sur EOF prématuré
//    (données tronquées) ou erreur d'écriture.
static int decompress_data(bit_reader *br, FILE *out, int iroot) {
  while (true) {
    int node = iroot;
    while (table_ascii[node].mark == NODE) {
      int bit = read_bit(br);
      if (bit < 0) {
        fprintf(stderr, "unexpected EOF in data\n");
        return -1;
      }
      node = table_ascii[node].next[bit];
    }
    if (table_ascii[node].mark == XLEAF) {
      return 0;
    }
    if (fputc(table_ascii[node].val, out) == EOF) {
      fprintf(stderr, "write error\n");
      return -1;
    }
  }
}

int uncompress_stream(FILE *in, FILE *out) {
  bit_reader br = { .f = in, .buffer = 0, .bit_count = 0, .eof = 0 };
  next_index = 0;
  int iroot = read_tree_rec(&br);
  if (iroot < 0) {
    fprintf(stderr, "malformed or truncated Huffman tree\n");
    return -1;
  }
  // Un arbre valide a soit une racine interne (au moins deux feuilles : au moins
  // un caractere + CASPER), soit une unique feuille CASPER (fichier source vide).
  // Une racine reduite a une feuille de donnees (non-CASPER) est malformee : la
  // decoder ferait boucler decompress_data a l'infini, reemettant le meme
  // caractere sans jamais consommer de bit ni atteindre la fin du flux. On rejette
  // donc ce cas explicitement (protection contre un deni de service / disque plein
  // declenche par un fichier de quelques octets).
  if (table_ascii[iroot].mark == LEAF) {
    fprintf(stderr, "malformed Huffman tree: lone data leaf\n");
    return -1;
  }
  return decompress_data(&br, out, iroot);
}
