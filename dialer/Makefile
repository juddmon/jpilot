CFLAGS=-O

all: jpilot-dial

jpilot-dial: jpilot-dial.o
	$(CC) $(CFLAGS) jpilot-dial.o -o jpilot-dial -lm

jpilot-dial.o: jpilot-dial.c
	$(CC) $(CFLAGS) -c jpilot-dial.c

clean:
	$(RM) jpilot-dial.o jpilot-dial *~
