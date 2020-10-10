SRC_DIR = $(CURDIR)/src

.PHONY: all
.PHONY: brcm-nand-bch
.PHONY: clean
.PHONY: decode
.PHONY: decodemod
.PHONY: encode
.PHONY: reencode

all:
	$(MAKE) -C "$(SRC_DIR)" all

brcm-nand-bch:
	$(MAKE) -C "$(SRC_DIR)" brcm-nand-bch

clean:
	$(MAKE) -C "$(SRC_DIR)" clean

decode:
	$(MAKE) -C "$(SRC_DIR)" decode

decodemod:
	$(MAKE) -C "$(SRC_DIR)" decodemod

encode:
	$(MAKE) -C "$(SRC_DIR)" encode

reencode:
	$(MAKE) -C "$(SRC_DIR)" reencode
