#ifndef DRV_NL10_OUTPUT
#define DRV_NL10_OUTPUT

#include <stdint.h>

void mylog(const char *format, ...);
#define log_warning mylog
#define log_error   mylog
#define log_message mylog


#define OUTPUT_FORMAT_BMP 0
#define OUTPUT_FORMAT_PDF 1

/* MAX_COL must be a multiple of 32 */
/* 2432 x 3172 */
#define BORDERX 16
#define BORDERY 2
#define MAX_COL (80 * 30 + 2 * BORDERX)
#define MAX_ROW (66 * 48 + 2 * BORDERY)

void output_open(const char *filename, int format);
void output_close();

void output_reset();
int  output_linefeed(int linespace);
void output_formfeed();
void output_draw_point2(int x, int y);
void output_draw_point3(int x, int y);

#endif
