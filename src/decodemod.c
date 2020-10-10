#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include "bch.h"

#define MIN_FIELD 13
#define MAX_FIELD 14
#define MIN_T 1
#define MAX_T 40
#define SECTORS_PER_PAGE 4

int ecc_level = 4; /* BCH 4 algorithm is used for ECC level. In schematic bootstrap sets to 4 bit ECC level for NAND. */
uint32_t block_size = 131072;
int spare_area = 64;
int field_order = 14; /* Field order for above 7.0 broadcom NAND Controller is 14, older is 13*/
int page_size = 2048;
int page_plus_obb_size = 0;

#define OOB_ECC_LEN 7 /* 7 bytes of ECC bits for BCH4, upto 70 bytes of ECC for t = 40, m = 14, BCH code */

#define MAX_BLOCK_SIZE       524288
#define MAX_OOB_BLOCK_SIZE   540672
#define BROADCOM_POLY        0x5803

/**
 *
 * This program reads raw NAND image from standard input and updates ECC bytes in the OOB block for each sector.
 * Data layout is as following:
 *
 * 2 KB page, consisting of 4 x 512 B sectors
 * 64 bytes OOB, consisting of 4 x 16 B OOB regions, one for each sector
 *
 * In each OOB region, the first 9 1/2 bytes are user defined and the remaining 6 1/2 bytes are ECC.
 *
 */

void print_usage(void)
{
    printf("decodemode 1.0\n");
    printf("Usage: nand-image-builder [OPTION]...\n");
    printf("\t -i input file\n");
    printf("\t -o output file\n");
    printf("\t -b block size\n");
    printf("\t -p page size\n");
    printf("\t -s spare area/oob area size\n");
    printf("\t -e ecc mode\n");
    printf("\t -m field order\n");
    printf("Default command is:\n");
    printf("./decodemode -i infile -o outfile -b 131072 -p 2048 -s 64 -e 4 -m 14\n");
    return;
}

void correct_bch(struct bch_control *bch, uint8_t *data, unsigned int len, unsigned int *errloc, int nerr) {
    for (int i = 0; i < nerr; ++i) {
        int bi = errloc[i];
        if (bi < 8 * len) {
            data[bi >> 3] ^= (1 << (bi & 7));
        }
    }
}

int main(int argc, char **argv)
{
    int option = 0;
    uint32_t blocks_app, blocks_num = 0;
    uint32_t code_length, parity, msg_length, data_bytes = 0;
    uint8_t *page_buffer;
    uint32_t countMask, count = 0;
    uint32_t numBytesRead, totalBytesRead = 0;
    uint32_t sector_sz, oob_sz, oob_ecc_len, oob_ecc_ofs = 0;
    int pages_in_block, i, j, k = 0;
    const char *input_file = NULL;
    const char *output_file = NULL;
    FILE *in_file = NULL, *out_file = NULL;
    struct stat st;
    // uint8_t *cmp_buffer;
    uint8_t *buffer;
    int erase_block = 1;
    uint8_t ecc[OOB_ECC_LEN];
    unsigned int numdecodeerrors = 0;

    /* Initialize BCH lib data and parameters */

    while ((option = getopt(argc, argv, "e:i:o:b:p:s:m:")) != -1)
    {
        switch (option)
        {
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'b':
            block_size = atoi(optarg);
            break;
        case 'p':
            page_size = atoi(optarg);
            break;
        case 'e':
            ecc_level = atoi(optarg);
            break;
        case 's':
            spare_area = atoi(optarg);
            break;
        case 'm':
            field_order = atoi(optarg);
            break;
        default:
            print_usage();
            goto exit;
        }
    }
    if (argc < 4)
    {
        print_usage();
        goto exit;
    }

    /* Validate input */
    if (block_size == 0)
    {
        printf("Block size can not be 0\n");
        goto exit;
    }
    if (block_size > MAX_BLOCK_SIZE)
    {
        printf("Block size to large\n");
    }

    if (page_size == 0)
    {
        printf("Page size can not be 0\n");
        goto exit;
    }
    if (page_size % 32)
    {
        printf("Page size not even multiple of 32\n");
        goto exit;
    }

    if (field_order < MIN_FIELD || field_order > MAX_FIELD)
    {
        printf("Field order %d and %d are  supported.\n", MIN_FIELD, MAX_FIELD);
        goto exit;
    }

    if (ecc_level < MIN_T || ecc_level > MAX_T)
    {
        printf("BCH level %d and %d are  supported.\n", MIN_T, MAX_T);
        goto exit;
    }

    if (input_file == NULL)
    {
        printf("-i input file name is missing\n");
        goto exit;
    }
    if (output_file == NULL)
    {
        printf("-o output file name is missing\n");
        goto exit;
    }

    in_file = fopen(input_file, "rb");
    if (in_file == NULL)
    {
        printf("In file open error\n");
        goto exit;
    }
    out_file = fopen(output_file, "wb");
    if (out_file == NULL)
    {
        printf("Out file open error\n");
        goto exit;
    }

    fseek(in_file, 0, SEEK_SET);
    fseek(out_file, 0, SEEK_SET);

    /* Validate that the input file size is a multiple of the block size */
    if (stat(input_file, &st) != 0)
    {
        printf("File size error\n");
        goto exit;
    }

    page_plus_obb_size = page_size + spare_area;

    blocks_app = st.st_size / block_size; /* calculate total blocks needed for application. */
    pages_in_block = block_size / page_size;

    sector_sz = page_size / SECTORS_PER_PAGE;
    oob_sz = spare_area / SECTORS_PER_PAGE;

    /* Codeword length (in bits) */
    code_length = (sector_sz + oob_sz) * 8;

    /* Parity length (in bits). */
    parity = field_order * ecc_level;

    /* Message length (in bits). */
    msg_length = code_length - parity;

    /* Number of data bytes = floor(k/8) */
    data_bytes = msg_length / 8;

    /* Number of ecc_code bytes, v = ceil(p/8) = ceil(m*t/8) = p_partial_page_size+s-u */
    oob_ecc_len = (sector_sz + oob_sz) - data_bytes;

    if (oob_ecc_len > oob_sz)
    {
        printf(" Number of spare bytes must be at least %d\n", oob_ecc_len);
        goto exit;
    }

    oob_ecc_ofs = oob_sz - oob_ecc_len;

    countMask = pages_in_block - 1;

    page_buffer = malloc((sector_sz + oob_sz) * SECTORS_PER_PAGE); //alloca 512 + 16 * 4 = 2112
    if (!page_buffer)
    {
        printf("Failed to allocate the  buffer\n");
        goto exit;
    }

    // cmp_buffer = malloc(sector_sz * SECTORS_PER_PAGE);
    // if (!cmp_buffer)
    // {
    //     printf("Failed to allocate the  buffer\n");
    //     goto exit;
    // }

    buffer = malloc(sector_sz + oob_ecc_ofs); //alloca 512 + ?
    if (!buffer)
    {
        printf("Failed to allocate the  buffer\n");
        goto exit;
    }

    /* Initialize BCH lib data and parameters */

    struct bch_control *bch = init_bch(field_order, ecc_level, BROADCOM_POLY);

    if (!bch)
        goto exit;

    printf("Input file %s\n", input_file);
    printf("Size of input file: %d\n", (int)st.st_size);
    printf("Block size: %d\n", block_size);
    printf("Input file blocks %d\n", blocks_app);
    printf("Page size: %d\n", page_size);
    printf("Pages in block: %d\n", pages_in_block);
    printf("ECC mode: %d\n", ecc_level);
    printf("OOB area size %d\n", spare_area);
    printf("Sub Page size %d\n", sector_sz);
    printf("OOB size per subpage %d\n", oob_sz);
    printf("OOB_ECC_LEN %d\n", oob_ecc_len);
    printf("OOB_ECC_OFS %d\n", oob_ecc_ofs);

    while (!feof(in_file))

    {
        memset(page_buffer, 0xff, ((sector_sz + oob_sz) * SECTORS_PER_PAGE));

        numBytesRead = fread(page_buffer, (size_t)1, (sector_sz + oob_sz)*SECTORS_PER_PAGE, in_file);
        totalBytesRead += numBytesRead;

        if (numBytesRead == 0)
        {
            printf("\nFread failed or file size is zero\n");
            goto exit;
        }

        if (numBytesRead != (sector_sz + oob_sz)*SECTORS_PER_PAGE)
            break; /* skip if bytes read from fread api is less than page size + oob_sz*/

        //memset(cmp_buffer, 0, sector_sz * SECTORS_PER_PAGE);

        //memcpy(cmp_buffer, page_buffer, (sector_sz)*SECTORS_PER_PAGE);

        //TODO capire se serve sta roba

        for (i = 0; i != sector_sz * SECTORS_PER_PAGE; ++i)
        {
            if (page_buffer[i] != 0xff)
            {
                erase_block = 0;
                break;
            }
        }

        for (i = 0; i != SECTORS_PER_PAGE; ++i)
        {
            uint8_t *sector_data = page_buffer + sector_sz * i;
            uint8_t *sector_oob = page_buffer + sector_sz * SECTORS_PER_PAGE + oob_sz * i;
            uint8_t *sector_oob_ecc = sector_oob + oob_ecc_ofs;

            if (!erase_block)
            {

                //uint8_t buffer[SECTOR_SZ + OOB_ECC_OFS];
                //memcpy(buffer, sector_data, SECTOR_SZ);
                //memcpy(buffer + SECTOR_SZ, sector_oob, OOB_ECC_OFS);

                memset(buffer, 0, sector_sz + oob_ecc_ofs);
                memcpy(buffer, sector_data, sector_sz);
                memcpy(buffer + sector_sz, sector_oob, oob_ecc_ofs);

                unsigned int errlocs[ecc_level];

                int nerr = decode_bch(bch, buffer, sector_sz + oob_ecc_ofs, sector_oob_ecc, NULL, NULL, &errlocs[0]);
                if (nerr < 0) {
                    numdecodeerrors++;
                    fprintf(stderr, "\ndecode_bch failed with error %d (sector %d of page %d)", nerr, i, count);
                    goto exit;
                } else if (nerr > 0) {
                    fprintf(stderr, "\n%d bit errors in sector %d of page %d:", nerr, i, count);
                    for (int j = 0; j < nerr; ++j) {
                        fprintf(stderr, " %d", errlocs[j]);
                    }
                    fprintf(stderr, "\n");

                    correct_bch(bch, sector_data, sector_sz, &errlocs[0], nerr);
                }

                /* Copy the result in its OOB block */
				//printf("\r\nB: %d %08Xh P: %d %08Xh S: %d ", blocks_num, (blocks_num) * 135168 , count, count * 2112, i+1);
				
				// for (k = 0; k != oob_ecc_len; ++k)
				// {
				// 	printf("%02X", ecc[k]); 
				// }
            }
        }
		
        // i = memcmp(cmp_buffer, page_buffer, (sector_sz)*SECTORS_PER_PAGE); /* compare the page buffer with cmp_buffer to verify page has not modified except oob spare size. */

        // if (i != 0)
        //     printf("\r\npage no %d and block no %d compare failed\n", count, blocks_num);

        fwrite(page_buffer, (sector_sz + oob_sz) * SECTORS_PER_PAGE, 1, out_file);

        count++;
        erase_block = 1;

        if (!(count & countMask))
        {
            /* printf("block No  %d\r\n", blocks_num); */
            blocks_num++;
        }
    }

    free(page_buffer);
    //free(cmp_buffer);
    free(buffer);

exit:
    if (in_file)
        fclose(in_file);
    if (out_file)
        fclose(out_file);
    exit(0);
}

