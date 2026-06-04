huffman_dir = ../huffman/unhcompress/
CC = gcc
CFLAGS = -std=c2x \
  -Wall -Wconversion -Werror -Wextra -Wpedantic -Wwrite-strings \
  -O0 -g \
  -I$(huffman_dir)
vpath %.c $(huffman_dir)
vpath %.h $(huffman_dir)
objects = main.o unhcompress.o
executable = unhcompress
makefile_indicator = .\#makefile\#

.PHONY: all clean

all: $(executable)

clean:
	$(RM) $(objects) $(executable)
	@$(RM) $(makefile_indicator)

$(executable): $(objects)
	$(CC) $(objects) -o $(executable)

main.o: main.c unhcompress.h
unhcompress.o: unhcompress.c unhcompress.h

include $(makefile_indicator)

$(makefile_indicator): makefile
	@touch $@
	@$(RM) $(objects) $(executable)
