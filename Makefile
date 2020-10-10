all: reencode decode decodemod brcm-nand-bch encode

clean:
	rm -f reencode decode decodemod brcm-nand-bch

reencode: reencode.c bch.c bch.h
	gcc -g -o reencode reencode.c bch.c -lws2_32

decode: decode.c bch.c bch.h
	gcc -g -o decode decode.c bch.c -lws2_32

decodemod: decodemod.c bch.c bch.h
	gcc -g -o decodemod decodemod.c bch.c -lws2_32

brcm-nand-bch: brcm-nand-bch.c bch.c bch.h
	gcc -g -o brcm-nand-bch brcm-nand-bch.c bch.c -lws2_32

encode: encode.c bch.c bch.h
	gcc -g -o encode encode.c bch.c -lws2_32

