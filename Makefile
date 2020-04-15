all: reencode decode

clean:
	rm -f reencode decode

reencode: reencode.c bch.c bch.h
	gcc -o reencode reencode.c bch.c

decode: decode.c bch.c bch.h
	gcc -o decode decode.c bch.c
