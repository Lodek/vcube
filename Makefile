all: vcube

vcube: src/vcube.o src/smpl.o src/rand.o
	$(LINK.c) -o $@ -Bstatic src/*.o -lm

smpl.o: src/smpl.c src/smpl.h
	$(COMPILE.c) -g src/smpl.c

vcube.o: src/vcube.c src/smpl.h
	$(COMPILE.c) -g src/vcube.c

rand.o: src/rand.c
	$(COMPILE.c) -g src/rand.c

clean:
	$(RM) src/*.o vcube
