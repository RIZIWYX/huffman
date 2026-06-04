#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hcompress.h"

//print_help :
// affiche sur la sortie standard l'aide
static void print_help(const char *progname) {
  printf("Usage: %s SRC [DEST]\n", progname);
  printf(
      "Compress the input file SRC using Huffman coding.\n"
      "The output is written to DEST if specified, to standard output "
      "otherwise.\n");
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    print_help(argv[0]);
    return EXIT_SUCCESS;
  }
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Invalid number of arguments.\n");
    print_help(argv[0]);
    return EXIT_FAILURE;
  }
  FILE *in = fopen(argv[1], "rb");
  if (!in) {
    fprintf(stderr, "%s", "fopen source id a probleme");
    return EXIT_FAILURE;
  }
  FILE *out = stdout;
  if (argc == 3) {
    out = fopen(argv[2], "wb");
    if (!out) {
      fprintf(stderr, "fopen destination");
      fclose(in);
      return EXIT_FAILURE;
    }
  }
  int rc = compress_stream(in, out);
  fclose(in);
  if (out != stdout) {
    fclose(out);
  }
  return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
