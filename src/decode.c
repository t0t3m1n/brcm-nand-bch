#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bch.h"

/**
 *
 * This program reads raw NAND image from standard input and updates the bytes in each
 * sector from the ECC bytes in the OOB block.
 *
 * Data layout is as following:
 *
 * 2 KB page, consisting of 4 x 512 B sectors
 * 64 bytes OOB, consisting of 4 x 16 B OOB regions, one for each sector
 *
 * In each OOB region, the first 9 1/2 bytes are user defined and the remaining 6 1/2 bytes are ECC.
 *
 */

#define BCH_T 4
#define BCH_N 14
#define SECTOR_SZ 512
#define OOB_SZ 16
#define SECTORS_PER_PAGE 4
#define OOB_ECC_OFS 9
#define OOB_ECC_LEN 7

/**
 * correct_bch - correct error locations as found in decode_bch
 * @bch,@data,@len,@errloc: same as a previous call to decode_bch
 * @nerr: returned from decode_bch
 *
 * From: https://github.com/mborgerding/bch_codec/blob/master/bch_codec.c
 */
void correct_bch(struct bch_control *bch, uint8_t *data, unsigned int len, unsigned int *errloc, int nerr) {
    for (int i = 0; i < nerr; ++i) {
        int bi = errloc[i];
        if (bi < 8 * len) {
            data[bi >> 3] ^= (1 << (bi & 7));
        }
    }
}

int main(int argc, char *argv[]) {
    unsigned poly = argc < 2 ? 0 : strtoul(argv[1], NULL, 0);

    struct bch_control *bch = init_bch(BCH_N, BCH_T, poly);
    if (!bch) {
        return -1;
    }

    uint8_t page_buffer[(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE];
    unsigned int numdecodeerrors = 0;
    unsigned int pagenum = 0;
    while (fread(page_buffer, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE, 1, stdin) == 1) {
        // Erased pages have ECC = 0xff .. ff even though there may be user bytes in the OOB region
        int erased_block = 1;
        unsigned i;
        for (i = 0; i != SECTOR_SZ * SECTORS_PER_PAGE; ++i) {
            if (page_buffer[i] != 0xff) {
                erased_block = 0;
                break;
            }
        }

        for (i = 0; i != SECTORS_PER_PAGE; ++i) {
            uint8_t *sector_data = page_buffer + SECTOR_SZ * i;
            const uint8_t *sector_oob = page_buffer + SECTOR_SZ * SECTORS_PER_PAGE + OOB_SZ * i;
            if (!erased_block) {
                // Concatenate input data
                uint8_t buffer[SECTOR_SZ + OOB_ECC_OFS];
                memcpy(buffer, sector_data, SECTOR_SZ);
                memcpy(buffer + SECTOR_SZ, sector_oob, OOB_ECC_OFS);

                // Consume ECC
                unsigned int errlocs[BCH_T];
                int nerr = decode_bch(bch, buffer, SECTOR_SZ + OOB_ECC_OFS, sector_oob, NULL, NULL, &errlocs[0]);
                if (nerr < 0) {
                    numdecodeerrors++;
                } else if (nerr > 0) {
                    fprintf(stderr, "%d bit errors in sector %d of page %d:", nerr, i, pagenum);
                    for (int j = 0; j < nerr; ++j) {
                        fprintf(stderr, " %d", errlocs[j]);
                    }
                    fprintf(stderr, "\n");

                    correct_bch(bch, sector_data, SECTOR_SZ, &errlocs[0], nerr);
                }
            }
        }

        fwrite(page_buffer, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE, 1, stdout);
        pagenum++;
    }
}
