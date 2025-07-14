#ifndef DRV_NL10_OUTPUT_BMP_H
#define DRV_NL10_OUTPUT_BMP_H

#include "drv-nl10-output.h"

void output_bmp_reset();
int  output_bmp_linefeed(int linespace);
void output_bmp_formfeed();
void output_bmp_draw_point2(int x, int y);
void output_bmp_draw_point3(int x, int y);

void output_bmp_open(const char *fname);
void output_bmp_close();

#endif
