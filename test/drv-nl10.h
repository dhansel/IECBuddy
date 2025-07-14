#ifndef DRV_NL10_H
#define DRV_NL10_H

#include <stdint.h>
#include "drv-nl10-output.h"

int  drv_nl10_open(unsigned int secondary);
void drv_nl10_close();
int  drv_nl10_putc(uint8_t b);

int  drv_nl10_formfeed();
int  drv_nl10_init(const char *outputname, int outputformat);
void drv_nl10_shutdown(void);
void drv_nl10_reset(void);

#endif
