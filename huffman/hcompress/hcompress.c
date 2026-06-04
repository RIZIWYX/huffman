#include "hcompress.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//Taille maximale du tableau (feuilles + nœuds)
#define SIZE ((UCHAR_MAX + 1) * 2 + 1)

//Indice de la feuille contenant le caractere fantome
#define CASPER (UCHAR_MAX + 1)

//poid de la feuille contenant le caractere fantome
#define CASPER_WGT 1

//pas de predecesseur (la racine)
#define ROOT -1

//pas de successeur (feuille)
#define LEAF -1

// Premier indice disponible pour les nœuds
#define INDEX_NODE (UCHAR_MAX + 2)

//Taille maximale de la chaîne de caractères représentant l'arbre en notation de
// Lukasiewicz.(2826)
#define TREE_REPR_MAX ((CASPER + 1) * (CHAR_BIT + 2) + CASPER)

// hauteur max arbre Huffman avec 257 feuilles
#define CODE_MAX_LEN 257

typedef struct {
  long int wgt;
  int prev;
  union {
    int next[2];
    int val;
  };
} hitem;

hitem table_ascii[SIZE];
char *codes_char[UCHAR_MAX + 2];

void init_table(void) {
  for (int i = 0; i < SIZE; i++) {
    table_ascii[i].wgt = 0;
    table_ascii[i].prev = ROOT;
    if (i <= CASPER) {
      table_ascii[i].val = i;
    } else {
      table_ascii[i].next[0] = LEAF;
      table_ascii[i].next[1] = LEAF;
    }
  }
  for (int i = 0; i <= CASPER; i++) {
    codes_char[i] = nullptr;
  }
}

void stock_in_table(FILE *stream) {
  int c;
  while ((c = fgetc(stream)) != EOF) {
    if (c <= UCHAR_MAX) {
      table_ascii[c].wgt++;
    }
  }
  table_ascii[CASPER].wgt = CASPER_WGT;
  table_ascii[CASPER].val = CASPER;
}

int compar(const void *a, const void *b) {
  const hitem *ha = (const hitem *) a;
  const hitem *hb = (const hitem *) b;
  if (ha->wgt < hb->wgt) {
    return -1;
  }
  if (ha->wgt > hb->wgt) {
    return 1;
  }
  return 0;
}

int skip_zeros() {
  for (int i = 0; i <= CASPER; i++) {
    if (table_ascii[i].wgt > 0) {
      return i;
    }
  }
  return CASPER + 1;
}

int node_construction(int ileaf) {
  int next_node = INDEX_NODE;
  int active_count = (CASPER + 1 - ileaf);
  while (active_count > 1) {
    int i1 = SIZE;
    int i2 = SIZE;
    long int min1 = LONG_MAX;
    long int min2 = LONG_MAX;
    for (int i = ileaf; i <= CASPER; i++) {
      if (table_ascii[i].prev == ROOT) {
        if (table_ascii[i].wgt < min1) {
          min2 = min1;
          i2 = i1;
          min1 = table_ascii[i].wgt;
          i1 = i;
        } else if (table_ascii[i].wgt < min2) {
          min2 = table_ascii[i].wgt;
          i2 = i;
        }
      }
    }
    for (int i = INDEX_NODE; i < next_node; i++) {
      if (table_ascii[i].prev == ROOT) {
        if (table_ascii[i].wgt < min1) {
          min2 = min1;
          i2 = i1;
          min1 = table_ascii[i].wgt;
          i1 = i;
        } else if (table_ascii[i].wgt < min2) {
          min2 = table_ascii[i].wgt;
          i2 = i;
        }
      }
    }
    table_ascii[next_node].wgt = min1 + min2;
    table_ascii[next_node].prev = ROOT;
    table_ascii[next_node].next[0] = i1;
    table_ascii[next_node].next[1] = i2;
    table_ascii[i1].prev = next_node;
    table_ascii[i2].prev = next_node;
    active_count--;
    next_node++;
  }
  for (int i = ileaf; i <= CASPER; i++) {
    if (table_ascii[i].prev == ROOT) {
      return i;
    }
  }
  for (int i = INDEX_NODE; i < next_node; i++) {
    if (table_ascii[i].prev == ROOT) {
      return i;
    }
  }
  return SIZE;
}

//  build_codes : parcourt récursivement l'arbre de Huffman dont node est le
//    nœud courant. Construit pour chaque feuille le code binaire correspondant
//    sous forme de chaîne de caractères '0'/'1' de longueur depth, stockée dans
//    codes_char[val] après allocation. buffer est un tableau de travail de
//    longueur au moins depth utilisé pour accumuler les bits courants. Renvoie
//    -1 en cas de dépassement de capacité, sans effet sur codes_char[val].
//    Renvoie 0 en cas de succès.
static int build_codes(int node, char *buffer, int depth) {
  if (node <= CASPER) {
    int val = table_ascii[node].val;
    codes_char[val] = malloc((size_t) (depth + 1));
    if (codes_char[val] == nullptr) {
      return -1;
    }
    memcpy(codes_char[val], buffer, (size_t) depth);
    codes_char[val][depth] = '\0';
    return 0;
  }
  buffer[depth] = '0';
  if (build_codes(table_ascii[node].next[0], buffer, depth + 1) != 0) {
    return -1;
  }
  buffer[depth] = '1';
  if (build_codes(table_ascii[node].next[1], buffer, depth + 1) != 0) {
    return -1;
  }
  return 0;
}

int generate_code(int iroot) {
  char buffer[CODE_MAX_LEN];
  return build_codes(iroot, buffer, 0);
}

//  write_tree : écrit dans buf à partir de la position *pos la représentation
//    de Lukasiewicz de l'arbre de Huffman dont node est le nœud courant :
//    '1' suivi de CHAR_BIT + 1 bits de la valeur val pour une feuille,
//    '0' suivi des représentations récursives des deux fils pour un nœud
//    interne. Met à jour *pos en conséquence. buf doit être suffisamment grand
//    pour accueillir la représentation complète.
static void write_tree(int node, char *buf, size_t *pos) {
  if (node <= CASPER) {
    buf[(*pos)] = '1';
    ++(*pos);
    int bits = CHAR_BIT + 1;
    int val = table_ascii[node].val;
    for (int b = bits - 1; b >= 0; b--) {
      buf[(*pos)] = ((val >> b) & 1) ? '1' : '0';
      ++(*pos);
    }
  } else {
    buf[(*pos)] = '0';
    ++(*pos);
    write_tree(table_ascii[node].next[0], buf, pos);
    write_tree(table_ascii[node].next[1], buf, pos);
  }
}

void lukas_tree_rep(int iroot, char *s) {
  size_t pos = 0;
  write_tree(iroot, s, &pos);
  s[pos] = '\0';
}

//  write_bits_to_file : écrit dans out les bits de la chaîne bits caractère
//    par caractère ('0' ou '1'), en les accumulant dans *buffer par paquets de
//    8. *bit_count désigne le nombre de bits déjà accumulés dans *buffer.
//    Écrit un octet dans out et réinitialise *buffer et *bit_count à zéro dès
//    que 8 bits sont accumulés. Termine le programme avec EXIT_FAILURE en cas
//    d'erreur d'écriture.
static int write_bits_to_file(FILE *out, const char *bits,
    unsigned char *buffer, int *bit_count) {
  for (const char *p = bits; *p; ++p) {
    *buffer = (*buffer << 1) | (*p == '1');
    ++(*bit_count);
    if (*bit_count == 8) {
      if (fputc(*buffer, out) == EOF) {
        fprintf(stderr, "%s", "write error");
        return -1;
      }
      *buffer = 0;
      *bit_count = 0;
    }
  }
  return 0;
}

int compress_stream(FILE *in, FILE *out) {
  if (in == stdin) {
    fprintf(stderr, "Error: cannot compress from stdin (need to read twice)\n");
    return -1;
  }
  init_table();
  stock_in_table(in);
  qsort(table_ascii, CASPER + 1, sizeof(hitem), compar);
  int ileaf = skip_zeros();
  int iroot = node_construction(ileaf);
  generate_code(iroot);
  if (fseek(in, 0, SEEK_SET) != 0) {
    fprintf(stderr, "fseek failed\n");
    return -1;
  }
  unsigned char buffer = 0;
  int bit_count = 0;
  char tree_repr[TREE_REPR_MAX];
  lukas_tree_rep(iroot, tree_repr);
  if (write_bits_to_file(out, tree_repr, &buffer, &bit_count)) {
    return -1;
  }
  int c;
  while ((c = fgetc(in)) != EOF) {
    if (write_bits_to_file(out, codes_char[c], &buffer, &bit_count) != 0) {
      return -1;
    }
  }
  if (ferror(in)) {
    fprintf(stderr, "%s", "read error");
    return -1;
  }
  if (write_bits_to_file(out, codes_char[CASPER], &buffer, &bit_count)) {
    return -1;
  }
  if (bit_count > 0) {
    buffer <<= (8 - bit_count);
    if (fputc(buffer, out) == EOF) {
      fprintf(stderr, "%s", "write error");
      return -1;
    }
  }
  free_codes();
  return 0;
}

void free_codes() {
  for (int i = 0; i <= CASPER; i++) {
    free(codes_char[i]);
    codes_char[i] = nullptr;
  }
}
