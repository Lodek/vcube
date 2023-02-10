all: vcube

vcube: src/vcube.o src/smpl.o src/rand.o src/main.o src/rand_init.o
	$(LINK.c) -o $@ -Bstatic src/*.o -lm

smpl.o: src/smpl.c src/smpl.h
	$(COMPILE.c) -g src/smpl.c

vcube.o: src/vcube.c src/smpl.h
	$(COMPILE.c) -g src/vcube.c

rand.o: src/rand.c
	$(COMPILE.c) -g src/rand.c

main.o: vcube.o
	$(COMPILE.c) -g src/main.c

rand_init.o:
	$(COMPILE.c) -g src/rand_init.c

clean:
	$(RM) src/*.o vcube
