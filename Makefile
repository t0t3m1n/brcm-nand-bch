all: reencode decode brcm-nand-bch

clean:
	rm -f reencode decode brcm-nand-bch

reencode: reencode.c bch.c bch.h
	gcc -g -o reencode reencode.c bch.c

decode: decode.c bch.c bch.h
	gcc -g -o decode decode.c bch.c

brcm-nand-bch: brcm-nand-bch.c bch.c bch.h
	gcc -g -o brcm-nand-bch brcm-nand-bch.c bch.c

