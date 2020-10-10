
SRC_DIR = $(CURDIR)/src
DIST_DIR = ../dist

all: brcm-nand-bch decode decodemod encode reencode

brcm-nand-bch: brcm-nand-bch.c bch.c bch.h
	gcc -g -o "$(DIST_DIR)/brcm-nand-bch" brcm-nand-bch.c bch.c -lws2_32

clean:
	rm -f "$(DIST_DIR)/brcm-nand-bch.exe" "$(DIST_DIR)/decode.exe" "$(DIST_DIR)/decodemod.exe" "$(DIST_DIR)/encode.exe" "$(DIST_DIR)/reencode.exe"

decode: decode.c bch.c bch.h
	gcc -g -o "$(DIST_DIR)/decode" decode.c bch.c -lws2_32

decodemod: decodemod.c bch.c bch.h
	gcc -g -o "$(DIST_DIR)/decodemod" decodemod.c bch.c -lws2_32

encode: encode.c bch.c bch.h
	gcc -g -o "$(DIST_DIR)/encode" encode.c bch.c -lws2_32

reencode: reencode.c bch.c bch.h
	gcc -g -o "$(DIST_DIR)/reencode" reencode.c bch.c -lws2_32