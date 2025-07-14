#include "drv-nl10-output-bmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>


#define DPI_X   300
#define DPI_Y   300
#define BUF_ROW (4 * 4 * 9 + 1)

/* MAX_COL must be a multiple of 32 */
/* 2432 x 3172 */
#define BORDERX 16
#define BORDERY 2
#define MAX_COL (80 * 30 + 2 * BORDERX)
#define MAX_ROW (66 * 48 + 2 * BORDERY)


// BMP header sizes
#define FILE_HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define COLOR_TABLE_SIZE 8  // 2 colors * 4 bytes each

static int pos_y, pos_y_pix;
static uint8_t linebuf[BUF_ROW][MAX_COL];

// Black and white palette
uint8_t bw_palette[8] = {
  0xFF, 0xFF, 0xFF, 0x00, // White (BGRA)
  0x00, 0x00, 0x00, 0x00  // Black (BGRA)
};




static void write_bitmap_file(const char *fname, int dpix, int dpiy, int width, int height, uint8_t *pixel_data)
{
  const int bits_per_pixel = 1;
  const int bytes_per_row_raw = (width * bits_per_pixel + 7) / 8; // 128 bytes

  // Rows are padded to multiples of 4 bytes
  const int row_stride = ((bytes_per_row_raw + 3) / 4) * 4; // 128 bytes (already multiple of 4)
  const int image_size = row_stride * height;

  const int file_size = FILE_HEADER_SIZE + INFO_HEADER_SIZE + COLOR_TABLE_SIZE + image_size;
  FILE *f = fopen(fname, "wb");
  if (!f) 
    {
      log_error("cannot open printer file to write: %s", fname);
      return;
    }
  log_message("writing printer output file: %s", fname);

  // ---- BMP FILE HEADER ----
  uint8_t file_header[FILE_HEADER_SIZE] = {
    'B', 'M',   // Signature
    0, 0, 0, 0, // File size
    0, 0,       // Reserved1
    0, 0,       // Reserved2
    0, 0, 0, 0  // Pixel data offset
  };

  // File size
  file_header[2] = (uint8_t)(file_size);
  file_header[3] = (uint8_t)(file_size >> 8);
  file_header[4] = (uint8_t)(file_size >> 16);
  file_header[5] = (uint8_t)(file_size >> 24);

  // Offset to pixel data
  int pixel_data_offset = FILE_HEADER_SIZE + INFO_HEADER_SIZE + COLOR_TABLE_SIZE;
  file_header[10] = (uint8_t)(pixel_data_offset);
  file_header[11] = (uint8_t)(pixel_data_offset >> 8);
  file_header[12] = (uint8_t)(pixel_data_offset >> 16);
  file_header[13] = (uint8_t)(pixel_data_offset >> 24);
  fwrite(file_header, 1, FILE_HEADER_SIZE, f);

  // ---- DIB HEADER (BITMAPINFOHEADER) ----
  uint8_t info_header[INFO_HEADER_SIZE] = {0};
  info_header[0] = INFO_HEADER_SIZE; // Header size
  info_header[4] = (uint8_t)(width);
  info_header[5] = (uint8_t)(width >> 8);
  info_header[6] = (uint8_t)(width >> 16);
  info_header[7] = (uint8_t)(width >> 24);
  info_header[8] = (uint8_t)(height);
  info_header[9] = (uint8_t)(height >> 8);
  info_header[10] = (uint8_t)(height >> 16);
  info_header[11] = (uint8_t)(height >> 24);
  info_header[12] = 1; // Planes
  info_header[14] = bits_per_pixel; // Bits per pixel
  // Compression (0 = BI_RGB, no compression)
  // Image size
  info_header[20] = (uint8_t)(image_size);
  info_header[21] = (uint8_t)(image_size >> 8);
  info_header[22] = (uint8_t)(image_size >> 16);
  info_header[23] = (uint8_t)(image_size >> 24);
  // X pixels per meter
  int ppm = dpix * 39.3701;
  info_header[24] = (uint8_t)(ppm);
  info_header[25] = (uint8_t)(ppm >> 8);
  info_header[26] = (uint8_t)(ppm >> 16);
  info_header[27] = (uint8_t)(ppm >> 24);
  // Y pixels per meter
  ppm = dpiy * 39.3701;
  info_header[28] = (uint8_t)(ppm);
  info_header[29] = (uint8_t)(ppm >> 8);
  info_header[30] = (uint8_t)(ppm >> 16);
  info_header[31] = (uint8_t)(ppm >> 24);
  // Colors in color table (2)
  info_header[32] = 2;
  // Important color count (0 = all)
  fwrite(info_header, 1, INFO_HEADER_SIZE, f);

  // ---- COLOR TABLE ----
  fwrite(bw_palette, 1, COLOR_TABLE_SIZE, f);

  // ---- PIXEL DATA ----
  fwrite(pixel_data, MAX_ROW*MAX_COL/8, 1, f);
  
  fclose(f);
}


static char *filename = NULL;
static int pixel_col = 0, pixel_row = 0, page_num = 0;
static uint8_t *pixel_data;


static int page_is_empty()
{
  if( pixel_row>0 || pixel_col>0 ) 
    for(int i=0; i<=pixel_row*MAX_COL/8; i++)
      if( pixel_data[i]!=0 )
        return 0;

  return 1;
}


static void output_page()
{
  if( filename && !page_is_empty() )
    {
      char namebuf[100];
      sprintf(namebuf, filename, ++page_num);
      write_bitmap_file(namebuf, DPI_X, DPI_Y, MAX_COL, MAX_ROW, pixel_data);
    }
}


static void output_nextrow()
{
  if( pixel_data )
    {
      pixel_col = 0;
      if( ++pixel_row==MAX_ROW )
        {
          output_page();
          memset(pixel_data, 0, MAX_ROW*MAX_COL/8);
          pixel_row = 0;
        }
    }
}


static void output_pixel(int black)
{
  if( pixel_col<MAX_COL )
    {
      if( black ) pixel_data[(MAX_ROW-pixel_row)*MAX_COL/8+pixel_col/8] |= 1 << (7-(pixel_col & 7));
      pixel_col++;
    }
}


static inline int inc_y()
{
    switch ((pos_y++) % 3) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 1;
    }

    return 0;
}


static void output_buf()
{
    int r, c;

    /* output buffer */
    for (r = 0; r < BUF_ROW; r++)
      {
        for (c = 0; c < MAX_COL; c++) output_pixel(linebuf[r][c]);
        output_nextrow();
      }

    /* clear buffer */
    memset(linebuf, 0, BUF_ROW * MAX_COL * sizeof(uint8_t));

    pos_y += (BUF_ROW / 4 * 3);
    pos_y_pix += BUF_ROW;
}


int output_bmp_linefeed(int linespace)
{
    int c, i, j, nextpage = 0;

    for (i = 0; i < linespace; i++) {
        for (j = inc_y(); j > 0; j--) {
            while (pos_y_pix < BORDERY) {
                output_nextrow();
                pos_y_pix++;
            }

            /* output topmost row */
            for (c = 0; c < MAX_COL; c++) output_pixel(linebuf[0][c]);
            output_nextrow();

            /* move everything else one row up */
            memmove(linebuf[0], linebuf[1], (BUF_ROW - 1) * MAX_COL * sizeof(uint8_t));

            /* clear bottom row */
            memset(linebuf[BUF_ROW - 1], 0, MAX_COL * sizeof(uint8_t));

            /* increase pixel row count */
            pos_y_pix++;

            /* check end-of-page */
            if (pos_y_pix >= MAX_ROW - BORDERY) {
                while (pos_y_pix++ < MAX_ROW) {
                  output_nextrow();
                }
                nextpage = 1;
                pos_y = 0;
                pos_y_pix = 0;
            }
        }
    }

    return nextpage;
}


void output_bmp_formfeed()
{
    int r;
    output_buf();
    for (r = pos_y_pix; r < MAX_ROW; r++)
      output_nextrow();
    pos_y = 0;
    pos_y_pix = 0;
}


#define valid_xpos(X)   ((X) >= 0 && (X) < MAX_COL)
#define valid_ypos(Y)   ((Y) >= 0 && (Y) < BUF_ROW)
#define valid_pos(X, Y) (valid_xpos(X) && valid_ypos(Y))


void output_bmp_draw_point2(int x, int y)
{
/*
   **
   #*
   **
*/

    if (valid_pos(x, y)) {
        linebuf[y][x] = 1;
    }
    if (valid_pos(x + 1, y)) {
        linebuf[y][x + 1] = 1;
    }
    if (valid_pos(x, y + 1)) {
        linebuf[y + 1][x] = 1;
    }
    if (valid_pos(x, y - 1)) {
        linebuf[y - 1][x] = 1;
    }
    if (valid_pos(x + 1, y + 1)) {
        linebuf[y + 1][x + 1] = 1;
    }
    if (valid_pos(x + 1, y - 1)) {
        linebuf[y - 1][x + 1] = 1;
    }
}


void output_bmp_draw_point3(int x, int y)
{
/*
    *
   *#*
    *
*/

    if (valid_pos(x, y)) {
        linebuf[y][x] = 1;
    }
    if (valid_pos(x - 1, y)) {
        linebuf[y][x - 1] = 1;
    }
    if (valid_pos(x + 1, y)) {
        linebuf[y][x + 1] = 1;
    }
    if (valid_pos(x, y - 1)) {
        linebuf[y - 1][x] = 1;
    }
    if (valid_pos(x, y + 1)) {
        linebuf[y + 1][x] = 1;
    }
}


void output_bmp_reset()
{
  memset(linebuf, 0, MAX_COL * BUF_ROW);
  pos_y = 0;
  pos_y_pix = 0;
}


void output_bmp_open(const char *fname)
{
  filename = strdup(fname);
  pixel_data = calloc(MAX_ROW*MAX_COL/8, 1);
  pixel_row = 0;
  pixel_col = 0;
  page_num  = 0;
    
}


void output_bmp_close()
{
  if( pixel_data!=NULL )
    {
      if( !page_is_empty() ) output_page();
      free(pixel_data);
      pixel_data = NULL;
    }


  if( filename ) free(filename);
}


