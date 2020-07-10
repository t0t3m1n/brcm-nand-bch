all: reencode decode decodemod brcm-nand-bch

clean:
	rm -f reencode decode decodemod brcm-nand-bch

reencode: reencode.c bch.c bch.h
	gcc -g -o reencode reencode.c bch.c

decode: decode.c bch.c bch.h
	gcc -g -o decode decode.c bch.c

decodemod: decodemod.c bch.c bch.h
	gcc -g -o decodemod decodemod.c bch.c

brcm-nand-bch: brcm-nand-bch.c bch.c bch.h
	gcc -g -o brcm-nand-bch brcm-nand-bch.c bch.c

