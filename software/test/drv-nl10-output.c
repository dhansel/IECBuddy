#include "drv-nl10-output.h"
#include "drv-nl10-output-bmp.h"
#include "drv-nl10-output-pdf.h"


static int output_format = OUTPUT_FORMAT_BMP;

int output_linefeed(int linespace)
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      return output_bmp_linefeed(linespace); 
    case OUTPUT_FORMAT_PDF:
      return output_pdf_linefeed(linespace); 
    }
}


void output_formfeed()
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_formfeed();
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_formfeed();
      break;
    }
}


void output_draw_point2(int x, int y)
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_draw_point2(x, y);
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_draw_point2(x, y);
      break;
    }
}


void output_draw_point3(int x, int y)
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_draw_point3(x, y);
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_draw_point3(x, y);
      break;
    }
}


void output_reset()
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_reset();
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_reset();
      break;
    }
}



void output_open(const char *fname, int format)
{
  output_format = format;

  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_open(fname);
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_open(fname);
      break;
    }
}


void output_close()
{
  switch( output_format )
    {
    case OUTPUT_FORMAT_BMP: 
      output_bmp_close();
      break;
    case OUTPUT_FORMAT_PDF:
      output_pdf_close();
      break;
    }
}

