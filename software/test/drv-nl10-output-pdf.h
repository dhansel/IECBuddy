#ifndef DRV_NL10_OUTPUT_PDF_H
#define DRV_NL10_OUTPUT_PDF_H

#include "drv-nl10-output.h"

void output_pdf_reset();
int  output_pdf_linefeed(int linespace);
void output_pdf_formfeed();
void output_pdf_draw_point2(int x, int y);
void output_pdf_draw_point3(int x, int y);

void output_pdf_open(const char *fname);
void output_pdf_close();

#endif
