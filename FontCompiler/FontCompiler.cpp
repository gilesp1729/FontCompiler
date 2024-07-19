// FontCompiler
// Read and process a GFX font file with character shapes in comments per the SymbolMono font.
// The example below is 16 bits wide (we only deal in whole bytes)

// character name                               // Name of character after double-slash
/*| 8 4 2 1 8 4 2 1 8 4 2 1 8 4 2 1 |*/         // Header is ignored
/*| . . . . . . . , . . . . . . . . |*/         // dot/comma for zero, X for one
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/
/*| . . . . . . . , . . . . . . . . |*/

// Append the corresponding hex codes to each line,
// and write out the glyph array with indexes and name as the end.
// Some hand editing may be required to get the final font file.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Glyph
{
    int index;
    int width;
    int height;
    int x_advance;
    int dx;
    int dy;
    char charname[80];
} Glyph;

// Scaling factor for bitmap. Must be greater than 1.0f.
const float factor = 2.0f;

// Scale up an image by interpolation. Src image is 8-bit, up to 32x32. Dst image may be taller (32x64)
void scale_image(float scale, int *nbits, int *nrows, unsigned char src_image[32][32], unsigned char dst_image[32][64])
{
    int dst_nbits, dst_nrows;

    dst_nrows = *nrows * scale;
    dst_nbits = *nbits * scale;

    for (int row = 0; row < dst_nrows; row++)
    {
        int src_row = row / scale;
        float row_frac = (row / scale) - src_row;
        int next_row = src_row + 1;
        if (next_row >= *nrows)
            next_row = *nrows - 1;

        for (int col = 0; col < dst_nbits; col++)
        {
            int src_col = col / scale;
            float col_frac = (col / scale) - src_col;
            int next_col = src_col + 1;
            if (next_col >= *nbits)
                next_col = *nbits - 1;

            dst_image[col][row] =
                src_image[src_col][src_row] * (1 - col_frac) * (1 - row_frac) +
                src_image[next_col][src_row] * (col_frac) * (1 - row_frac) +
                src_image[src_col][next_row] * (1 - col_frac) * (row_frac)+
                src_image[next_col][next_row] * (col_frac) * (row_frac);
        }
    }
    *nrows = dst_nrows;
    *nbits = dst_nbits;
}


int main(int argc, char* argv[])
{
    FILE* input;
    FILE* output;
    char fontname[64];
    char line[160], saved_line[160];
    bool new_glyph = true;
    char* tok, *ctx;
    unsigned char src_image[32][32], dst_image[32][64];
    unsigned long bits = 0;
    int nbits, i;
    int nrows = 0, nglyphs = 0, indx = 0, y_advance = 0, rightmost = 0;
    int threshold = 0x80;
    Glyph   glyphs[256];

    if (argc < 2)
    {
        printf("Insufficient arguments. Usage: FontCompiler <input> <output>\n");
        exit(1);
    }
    if (fopen_s(&input, argv[1], "r") != 0)
    {
        printf("Cannot open input file.\n");
        exit(1);
    }

    // Start the output file with a bitmaps declaration. Remove ".h" from 
    // the output file name first.
    fopen_s(&output, argv[2], "w");
    strcpy_s(fontname, argv[2]);
    fontname[strlen(fontname) - 2] = '\0';
    fprintf(output, "#include <gfxfont.h>\n\n");
    fprintf(output, "const uint8_t %sBitmaps[] PROGMEM = {\n", fontname);

    // Get in the first line. Remove any trailing line ends.
    fgets(line, 160, input);
    line[strlen(line) - 1] = '\0';

    while (1)
    {
        // Search forward for the introducer "/*|". When found, remember the previous
        // "//" comment line for the character name.
        if (line[0] == '/' && line[1] == '/')
        {
            strcpy_s(glyphs[nglyphs].charname, &line[2]);
            new_glyph = true;
        }

        nrows = 0;
        rightmost = 0;
        memset(src_image, 0, 32 * 32);

        while (line[0] == '/' && line[1] == '*' && line[2] == '|')
        {
            // Output the charname
            if (new_glyph)
            {
                fprintf(output, "// %s\n", glyphs[nglyphs].charname);
                new_glyph = false;
            }

            // Copy line to save it from strtok
            strcpy_s(saved_line, line);

            tok = strtok_s(&line[3], " ", &ctx);

            // Skip lines that look like "/*| 8 4 2 1 .... ".
            if (*tok != '8')
            {
                // Process lines with X dot or comma until trailing "|*/" is found.
                nbits = 0;
                while (*tok != '|')
                {
                    if (*tok == 'X' || *tok == 'x')
                        src_image[nbits][nrows] = 0xFF;
                    nbits++;
                    tok = strtok_s(NULL, " ", &ctx);
                }

                nrows++;
            }

            // Go back for the next line
            fgets(line, 160, input);
            line[strlen(line) - 1] = '\0';
        }

        // End of glyph. Write out the comment (picture of glyph) and the hex values.
        // Remember the number of rows and the offset to the glyph.
        if (nrows != 0)
        {
            // At this point we can do any manipulations of the glyph bitmap
            // (scaling, etc)

            scale_image(factor, &nbits, &nrows, src_image, dst_image);

            // Output the 8 4 2 1 ... line
            fprintf(output, "/*| ");
            for (i = 0; i < nbits / 4; i++)
                fprintf(output, "8 4 2 1 ");
            fprintf(output, "|*/\n");

            // Output the bits in the row
            for (int row = 0; row < nrows; row++)
            {
                fprintf(output, "/*| ");
                bits = 0;
                for (i = 0; i < nbits; i++)
                {
                    //int bit = (bits[row] >> (i - 1)) & 0x1;
                    int bit = dst_image[i][row] > threshold;
                    bits <<= 1;
                    bits |= bit;
                    if (bit == 0)
                    {
                        fprintf(output, "%s ", (((i + 1) % 8) || i == nbits - 1) ? "." : ",");
                    }
                    else
                    {
#if 1
                        fprintf(output, "X ");
#else
                        // Print the high nibble of the interpolated image for testing
                        fprintf(output, "%X ", dst_image[i][row] >> 4);
#endif
                        if (i > rightmost)
                            rightmost = i;
                    }
                }

                // Output the hex for groups of 8 bits in the row
                fprintf(output, "|*/");
                for (i = nbits; i > 0; i -= 8)
                    fprintf(output, " 0x%02X,", (bits >> (i - 8)) & 0xFF);
                fprintf(output, "\n");
            }

            glyphs[nglyphs].index = indx;
            glyphs[nglyphs].width = nbits;
            glyphs[nglyphs].height = nrows;
            //glyphs[nglyphs].x_advance = nbits + 3;      // Arbitrary
            glyphs[nglyphs].x_advance = rightmost + 1 + 3;      // Arbitrary
            glyphs[nglyphs].dx = 3;                    // Arbitrary
            glyphs[nglyphs].dy = -(nrows + 3);         // Restrict to width+3 if it's too big (push into descender)
            if (glyphs[nglyphs].dy < -(nbits + 3))
                glyphs[nglyphs].dy = -(nbits + 3);
            nglyphs++;

            // Accumulate the tallest character height
            if (nrows > y_advance)
                y_advance = nrows;

            // Index to next character glyph bitmap
            indx += nrows * nbits / 8;

            // Ready the image array for the next glyph bitmap
            nrows = 0;
            rightmost = 0;
            memset(src_image, 0, 32 * 32);
        }

        // Check for // introducing next glyph
        if (line[0] == '/' && line[1] == '/')
        {
            strcpy_s(glyphs[nglyphs].charname, &line[2]);
            new_glyph = true;
        }

        // Go back for the next line
        fgets(line, 160, input);
        line[strlen(line) - 1] = '\0';

        // Stop when we encounter the closing brace
        if (strchr(line, '}') != NULL)
            break;
    }

    fclose(input);

    // Write out the glyphs declaration. Terminate the bitmaps array first.
    // Put in one extra byte since the last glyph bitmap ended with a comma.
    fprintf(output, "0x00};\n\n");
    fprintf(output, "const GFXglyph %sGlyphs[] PROGMEM = {\n", fontname);

    // Pad the glyphs array out to 32 entries (0-31) with copies of glyph 0.
    // This is for symbol fonts so they don't leave holes before the space
    // character in text fonts.
    if (nglyphs < 32)
    {
        for (i = nglyphs; i < 32; i++)
        {
            glyphs[i] = glyphs[0];
            sprintf_s(glyphs[i].charname, "%d", i);
        }
        nglyphs = 32;
    }

    // Write out the glyphs array 
    for (i = 0; i < nglyphs; i++)
    {
        // Index,  W, H,xAdv,dX, dY       char name
        // {   0, 16, 21, 21, 3, -19}, // 00 test square
        fprintf(output, "  { %4d, %2d, %2d, %2d, %2d, %3d }%s // %s\n",
            glyphs[i].index,
            glyphs[i].width,
            glyphs[i].height,
            glyphs[i].x_advance,
            glyphs[i].dx,
            glyphs[i].dy,
            (i == nglyphs - 1) ? "};" : ",",    // last one gets a closing brace
            glyphs[i].charname);
    }

    // Finish it off with the font declaration.

    fprintf(output, "const GFXfont %s PROGMEM = {\n", fontname);
    fprintf(output, "  (uint8_t *)%sBitmaps,\n", fontname);
    fprintf(output, "  (GFXglyph*)%sGlyphs,\n", fontname);
    fprintf(output, "  0, %d, %d};\n", nglyphs - 1, y_advance + 10);     // Arbitrary gap in y
    fclose(output);
}

