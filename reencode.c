#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bch.h"

/**
 *
 * This program reads raw NAND image from standard input and updates ECC bytes
 * in the OOB block for each sector.
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

int main(int argc, char *argv[]) {
    unsigned poly = argc < 2 ? 0 : strtoul(argv[1], NULL, 0);

    struct bch_control *bch = init_bch(BCH_N, BCH_T, poly);
    if (!bch) {
        return -1;
    }

    uint8_t page_buffer[(SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE];
    unsigned int differing = 0;
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
            const uint8_t *sector_data = page_buffer + SECTOR_SZ * i;
            uint8_t *sector_oob = page_buffer + SECTOR_SZ * SECTORS_PER_PAGE + OOB_SZ * i;
            if (erased_block) {
                // Erased page ECC consumes full 7 bytes, including high 4 bits set to 0xf
                memset(sector_oob + OOB_ECC_OFS, 0xff, OOB_ECC_LEN);
            } else {
                // Concatenate input data
                uint8_t buffer[SECTOR_SZ + OOB_ECC_OFS];
                // buffer[0] = 0;
                memcpy(buffer, sector_data, SECTOR_SZ);
                memcpy(buffer + SECTOR_SZ, sector_oob, OOB_ECC_OFS);

                // Compute ECC
                uint8_t ecc[OOB_ECC_LEN];
                memset(ecc, 0, OOB_ECC_LEN);
                encode_bch(bch, buffer, SECTOR_SZ + OOB_ECC_OFS, ecc);

                // Was this sector's recomputed ECC different from the ECC already present?
                if (memcmp(ecc, sector_oob + OOB_ECC_OFS, OOB_ECC_LEN) != 0) {
                    fprintf(stderr, "errors in sector %d of page %d\n", i, pagenum);
                    differing++;
                }

                // Copy the result in its OOB block
                memcpy(sector_oob + OOB_ECC_OFS, ecc, OOB_ECC_LEN);
            }
        }

        fwrite(page_buffer, (SECTOR_SZ + OOB_SZ) * SECTORS_PER_PAGE, 1, stdout);

        pagenum++;
    }

    fprintf(stderr, "poly %d different OOB sectors: %d\n", poly, differing);
}
