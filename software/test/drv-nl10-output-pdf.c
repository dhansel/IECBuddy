#include "drv-nl10-output-pdf.h"

#include <stdio.h>
#include <stdlib.h>

// PDF units are points: 1 point = 1/72 inch

// Standard A4 page: 21 x 29.7 cm = 595 x 842 points (portrait)
const double page_width  = 595.0;
const double page_height = 842.0;

// Print head specifications
#define NEEDLE_SIZE      0.95  // in points
#define NEEDLE_SPACING   1.0   // in points


static FILE *pdf_f = NULL;
static long  pdf_object_offsets[1000+10], pdf_stream_start;
static int   pdf_num_objects = 0;

static double pos_y = 0; // in points (1 point = 1/72 inch)
static double scale_x = page_width / MAX_COL;


// x and y coordinates in points
void pdf_draw_dot_at(double x, double y) 
{
  if( pdf_f )
    fprintf(pdf_f, "%.2f %.2f m\n%.2f %.2f l\n", x, y, x, y);
}


static void pdf_page_start()
{                                               
  if( pdf_f )
    {
      pdf_object_offsets[pdf_num_objects++] = ftell(pdf_f);
      fprintf(pdf_f, "%03d 0 obj\n"
              "<< /Type /Page /Parent     0 R /MediaBox [0 0 %.2f %.2f] /Contents %d 0 R >>\n"
              "endobj\n",
              pdf_num_objects, page_width, page_height, pdf_num_objects+1);

      pdf_object_offsets[pdf_num_objects++] = ftell(pdf_f);
      fprintf(pdf_f, 
              "%03d 0 obj\n"
              "<< /Length              >>\n"
              "stream\n",
              pdf_num_objects);
      
      pdf_stream_start = ftell(pdf_f);
      fprintf(pdf_f, 
              "q\n"
              "%.2f w\n"
              "1 J\n",
              NEEDLE_SIZE);

      pos_y = 0;
    }
}


static void pdf_page_end()
{
  if( pdf_f )
    {
      fprintf(pdf_f, "S\nQ\n");
      int stream_len = ftell(pdf_f)-pdf_stream_start;
      fprintf(pdf_f, 
              "endstream\n"
              "endobj\n");

      fseek(pdf_f, pdf_object_offsets[pdf_num_objects-1]+21, SEEK_SET);
      fprintf(pdf_f, "%d", stream_len);
      fseek(pdf_f, 0, SEEK_END);
    }
}


int output_pdf_linefeed(int linespace)
{
  pos_y += linespace / 3.0;
  
  if( pos_y + (NEEDLE_SPACING * 9) > page_height )
    {
      pdf_page_end();
      pdf_page_start();
      return 1;
    }
  else
    return 0;
}


void output_pdf_formfeed()
{
  pdf_page_end();
  pdf_page_start();
  pos_y = 0;
}


void output_pdf_draw_point2(int x, int y)
{
  // y is "needle number" * 4
  pdf_draw_dot_at((x+0.5)*scale_x, page_height - pos_y - ((y / 4.0) * NEEDLE_SPACING));
}


void output_pdf_draw_point3(int x, int y)
{
  // y is "needle number" * 4
  pdf_draw_dot_at(x*scale_x, page_height - pos_y - ((y / 4.0) * NEEDLE_SPACING));
}


void output_pdf_reset()
{
  pos_y = 0;
}


void output_pdf_open(const char *fname)
{
  pdf_f = fopen(fname, "wb+");
  if( pdf_f )
    {
      // header
      fprintf(pdf_f, "%%PDF-2.0\n");
      
      // start first page
      pdf_page_start();
    }
}


void output_pdf_close()
{
  if( pdf_f )
    {
      pdf_page_end();

      // Catalog
      pdf_object_offsets[pdf_num_objects++] = ftell(pdf_f);
      fprintf(pdf_f, "%03d 0 obj\n<< /Type /Catalog /Pages %d 0 R >>\nendobj\n", pdf_num_objects, pdf_num_objects+1);
      
      // Pages
      pdf_object_offsets[pdf_num_objects++] = ftell(pdf_f);

      fprintf(pdf_f, "%03d 0 obj\n<< /Type /Pages /Kids [", pdf_num_objects);
      for(int i=0; i<pdf_num_objects-2; i+=2) fprintf(pdf_f, "%d 0 R ", i+1);
      fprintf(pdf_f, "] /Count %d >>\nendobj\n", pdf_num_objects/2-1);

      for(int i=0; i<pdf_num_objects-2; i+=2) 
        {
          fseek(pdf_f, pdf_object_offsets[i]+33, SEEK_SET);
          fprintf(pdf_f, "%d ", pdf_num_objects);
        }
      fseek(pdf_f, 0, SEEK_END);
      
      // XRef table
      long xref_offset = ftell(pdf_f);
      fprintf(pdf_f, "xref\n0 %d\n", pdf_num_objects+1);
      fprintf(pdf_f, "0000000000 65535 f \n"); // object 0 is free
      for(int i = 0; i < pdf_num_objects; ++i)
        fprintf(pdf_f, "%010ld 00000 n \n", pdf_object_offsets[i]);
  
      // Trailer
      fprintf(pdf_f,
              "trailer\n"
              "<< /Size %d /Root %d 0 R >>\n"
              "startxref\n"
              "%ld\n"
              "%%%%EOF\n", pdf_num_objects+1, pdf_num_objects-1, xref_offset);
  
      fclose(pdf_f);
      pdf_f = NULL;
    }
}
