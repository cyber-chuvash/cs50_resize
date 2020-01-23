// Resizes a BMP file

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "bmp.h"

int main(int argc, char *argv[])
{
    // ensure proper usage
    if (argc != 4)
    {
        fprintf(stderr, "Usage: resize f infile outfile\n");
        return 1;
    }

    double rfactor_i;
    double rfactor_f = modf(atof(argv[1]), &rfactor_i);

    // remember filenames
    char *infile = argv[2];
    char *outfile = argv[3];

    // open input file
    FILE *inptr = fopen(infile, "r");
    if (inptr == NULL)
    {
        fprintf(stderr, "Could not open %s.\n", infile);
        return 2;
    }

    // open output file
    FILE *outptr = fopen(outfile, "w");
    if (outptr == NULL)
    {
        fclose(inptr);
        fprintf(stderr, "Could not create %s.\n", outfile);
        return 3;
    }

    // read infile's BITMAPFILEHEADER
    BITMAPFILEHEADER inp_bmpfileheader;
    fread(&inp_bmpfileheader, sizeof(BITMAPFILEHEADER), 1, inptr);

    // read infile's BITMAPINFOHEADER
    BITMAPINFOHEADER inp_bmpinfoheader;
    fread(&inp_bmpinfoheader, sizeof(BITMAPINFOHEADER), 1, inptr);

    // ensure infile is (likely) a 24-bit uncompressed BMP 4.0
    if (inp_bmpfileheader.bfType != 0x4d42 || inp_bmpfileheader.bfOffBits != 54 || inp_bmpinfoheader.biSize != 40 ||
        inp_bmpinfoheader.biBitCount != 24 || inp_bmpinfoheader.biCompression != 0)
    {
        fclose(outptr);
        fclose(inptr);
        fprintf(stderr, "Unsupported file format.\n");
        return 4;
    }

    BITMAPFILEHEADER out_bmpfileheader = inp_bmpfileheader;
    BITMAPINFOHEADER out_bmpinfoheader = inp_bmpinfoheader;

    double horiz_rfactor_f = round(inp_bmpinfoheader.biWidth * rfactor_f) / (double) inp_bmpinfoheader.biWidth;
    double vert_rfactor_f = round(inp_bmpinfoheader.biHeight * rfactor_f) / (double) inp_bmpinfoheader.biHeight;

    // Calculate output file dimensions
    out_bmpinfoheader.biWidth = (int) round(inp_bmpinfoheader.biWidth * (rfactor_i + horiz_rfactor_f));
    out_bmpinfoheader.biHeight = (int) round(inp_bmpinfoheader.biHeight * (rfactor_i + vert_rfactor_f));

    int out_padding = (4 - (out_bmpinfoheader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    out_bmpinfoheader.biSizeImage = (
            (sizeof(RGBTRIPLE) * out_bmpinfoheader.biWidth) + out_padding
            ) * abs(out_bmpinfoheader.biHeight);

    out_bmpfileheader.bfSize = out_bmpinfoheader.biSizeImage + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // write outfile's BITMAPFILEHEADER
    fwrite(&out_bmpfileheader, sizeof(BITMAPFILEHEADER), 1, outptr);

    // write outfile's BITMAPINFOHEADER
    fwrite(&out_bmpinfoheader, sizeof(BITMAPINFOHEADER), 1, outptr);

    // determine padding for scanlines
    int inp_padding = (4 - (inp_bmpinfoheader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    int repeats;

    RGBTRIPLE prev_row [out_bmpinfoheader.biWidth];
    int prev_row_index = 0;

    // iterate over infile's scanlines
    for (int i = 0, biHeight = abs(inp_bmpinfoheader.biHeight); i < biHeight; i++) {
        if ((rfactor_i != 0) ||
            ((vert_rfactor_f <= 0.5) && (fabs(round(i * vert_rfactor_f) - (i * vert_rfactor_f)) <= 0.0001)) ||
            ((vert_rfactor_f > 0.5) && (fabs(round(i * vert_rfactor_f) - (i * vert_rfactor_f)) >= 0.0001))
        ) {
            // iterate over pixels in scanline
            for (int j = 0; j < inp_bmpinfoheader.biWidth; j++) {
                // temporary storage
                RGBTRIPLE triple;

                // read RGB triple from infile
                fread(&triple, sizeof(RGBTRIPLE), 1, inptr);

                if ((horiz_rfactor_f != .0) && (
                        ((horiz_rfactor_f <= 0.5) &&
                         (fabs(round(j * horiz_rfactor_f) - (j * horiz_rfactor_f)) <= 0.0001)) ||
                        ((horiz_rfactor_f > 0.5) &&
                         (fabs(round(j * horiz_rfactor_f) - (j * horiz_rfactor_f)) >= 0.0001))
                )) {
                    repeats = (int) rfactor_i + 1;
                } else {
                    repeats = (int) rfactor_i;
                }

                // write RGB triple to outfile
                for (int k = 0; k < repeats; k++) {
                    prev_row[prev_row_index] = triple;
                    prev_row_index++;
                    fwrite(&triple, sizeof(RGBTRIPLE), 1, outptr);
                }
            }

            prev_row_index = 0;

            // skip over padding in the input file, if any
            fseek(inptr, inp_padding, SEEK_CUR);

            // write padding to output file
            for (int k = 0; k < out_padding; k++) {
                fputc(0x00, outptr);
            }

            for (int l = 1; l < rfactor_i; l++) {
                fwrite(prev_row, sizeof(prev_row), 1, outptr);

                for (int k = 0; k < out_padding; k++) {
                    fputc(0x00, outptr);
                }
            }

            if ((rfactor_i != 0) && (vert_rfactor_f != 0) && (
                    ((vert_rfactor_f <= 0.5) && (fabs(round(i * vert_rfactor_f) - (i * vert_rfactor_f)) <= 0.0001)) ||
                    ((vert_rfactor_f > 0.5) && (fabs(round(i * vert_rfactor_f) - (i * vert_rfactor_f)) >= 0.0001))
            )) {
                fwrite(prev_row, sizeof(prev_row), 1, outptr);

                for (int k = 0; k < out_padding; k++) {
                    fputc(0x00, outptr);
                }
            }
        } else {
            fseek(inptr, inp_bmpinfoheader.biBitCount / 8 * inp_bmpinfoheader.biWidth + inp_padding, SEEK_CUR);
        }
    }

    // close infile
    fclose(inptr);

    // close outfile
    fclose(outptr);

    // success
    return 0;
}
