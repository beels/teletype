.PHONY: clean
CFLAGS=-std=c99 -g -Wall -fno-common -DSIM -I. -I../src -I../libavr32/src
DEPS =
OBJ = dm.o tele.o dmapi.o ../src/diskmenu.o \
	  ../src/teletype.o ../src/command.o ../src/helpers.o \
	../src/every.o ../src/match_token.o ../src/scanner.o \
	../src/mergesort.o ../src/filename.o \
	../src/scale.o ../src/scene_serialization.o \
	../src/state.o ../src/table.o \
	../libavr32/src/util.o ../libavr32/src/random.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dm: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

../src/match_token.c: ../src/match_token.rl
	ragel -C -G2 ../src/match_token.rl -o ../src/match_token.c

../src/scanner.c: ../src/scanner.rl
	ragel -C -G2 ../src/scanner.rl -o ../src/scanner.c

clean:
	rm -f dm
	rm -rf dm.dSYM
	rm -f *.o
	rm -f ../src/*.o
	rm -f ../src/ops/*.o
	rm -f ../libavr32/src/euclidean/*.o
	rm -f ../libavr32/src/*.o
	rm -f ../src/match_token.c
	rm -f ../src/scanner.c
