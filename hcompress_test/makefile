huffman_dir = ../huffman/hcompress/
CC = gcc
CFLAGS = -std=c2x \
  -Wall -Wconversion -Werror -Wextra -Wpedantic -Wwrite-strings \
  -O0 -g \
  -I$(huffman_dir)
vpath %.c $(huffman_dir)
vpath %.h $(huffman_dir)
objects = main.o hcompress.o
executable = hcompress
makefile_indicator = .\#makefile\#

.PHONY: all clean

all: $(executable)

clean:
	$(RM) $(objects) $(executable)
	@$(RM) $(makefile_indicator)

$(executable): $(objects)
	$(CC) $(objects) -o $(executable)

main.o: main.c hcompress.h
hcompress.o: hcompress.c hcompress.h

include $(makefile_indicator)

$(makefile_indicator): makefile
	@touch $@
	@$(RM) $(objects) $(executable)
