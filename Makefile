#set PILOT_DIR to where you installed pilot-link
PILOT_DIR=/usr/extra/pilot
#set CC to your compiler
CC = gcc
#you should be all done
PILOT_LIB = -L$(PILOT_DIR)/lib -lpisock
PILOT_INCLUDE = -I$(PILOT_DIR)/include

all: jpilot

jpilot: jpilot.o datebook.o address.o todo.o memo.o \
	datebook_gui.o address_gui.o todo_gui.o memo_gui.o \
	utils.o
	$(CC) $(PILOT_LIB) `gtk-config --cflags` `gtk-config --libs` \
	jpilot.o datebook.o address.o todo.o memo.o utils.o \
	datebook_gui.o address_gui.o todo_gui.o memo_gui.o -o jpilot

jpilot.o: jpilot.c datebook.h utils.h utils.o
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c jpilot.c

datebook.o: datebook.c datebook.h utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c datebook.c

address.o: address.c address.h utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c address.c

todo.o: todo.c todo.h utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c todo.c

memo.o: memo.c utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c memo.c

datebook_gui.o: datebook_gui.c utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c datebook_gui.c

address_gui.o: address_gui.c address.o
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c address_gui.c

todo_gui.o: todo_gui.c todo.o utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c todo_gui.c

memo_gui.o: memo_gui.c utils.h
	$(CC) $(PILOT_INCLUDE) `gtk-config --cflags` -c memo_gui.c

utils.o: utils.c utils.h
	$(CC) $(PILOT_INCLUDE) \
	`gtk-config --cflags` \
	-c utils.c

clean: 
	rm -f *.o *~

#
#Some other stuff
#
ttt: ttt.c
	$(CC) `gtk-config --cflags` `gtk-config --libs` ttt.c -o ttt

dump_datebook: dump_datebook.c 
	$(CC) $(PILOT_LIB) $(PILOT_INCLUDE) dump_datebook.c -o dump_datebook

dump_address: dump_address.c 
	$(CC) $(PILOT_LIB) $(PILOT_INCLUDE) utils.o dump_address.c -o dump_address

dump_todo: dump_todo.c 
	$(CC) $(PILOT_LIB) $(PILOT_INCLUDE) utils.o dump_todo.c -o dump_todo

install-datebook: install-datebook.c 
	$(CC) $(INCLUDES) $(PILOT_LIB) install-datebook.c -o install-datebook
