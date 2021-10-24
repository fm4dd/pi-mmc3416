CC=gcc
CFLAGS= -O3 -Wall -g
LIBS= -lm
AR=ar

ALLBIN=getmmc3416

all: ${ALLBIN}

clean:
	rm -f *.o ${ALLBIN}

getmmc3416: i2c_mmc3416.o getmmc3416.o
	$(CC) i2c_mmc3416.o getmmc3416.o -o getmmc3416 ${LIBS}

