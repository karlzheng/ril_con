
#CROSS_COMPILE := ""
#CC := gcc
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

ril_con:ril_con.c Makefile
	$(CC) -I include/ ril_con.c -L lib -l readline -l history -l ncurses -static -o ril_con 

ril_con_pc:ril_con.c Makefile
	gcc ril_con.c -L /usr/lib/ -l readline -l termcap -static -o ril_con_pc
