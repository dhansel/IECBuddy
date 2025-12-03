#include "IECDisplay_ST7789.h"
#ifdef SUPPORT_ST7789

#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <algorithm>
#include <cctype>
#include "src/AnimatedGif/AnimatedGIF.h"
using namespace std;

// defined at the bottom of this file
extern const GFXfont cbmFont;

// Define location where background image data gets buffered:
// If IMGDATA_FILEBUF is defined then data will be buffered in FLASH (LittleFS)
//    Value of IMGDATA_FILEBUF is filename under which the data will be stored
//    Pro: does not occupy any RAM, text running over image gets cleared properly
//    Con: slower than the other methods (because writing to FLASH takes time)
// If IMGDATA_MEMBUF is defined then data will be buffered in RAM
//    Value of IMGDATA_MEMBUF is the chunk size for memory allocation.
//    We must allocate memory in small-ish chunks, otherwise displaying a medium-sized
//    image will (after freeing) produce a memory "hole" that fragments memory such
//    that there is possibly not enough contiguous memory for a bigger image.
//    Pro: fast, text running over image gets cleared properly
//    Con: image data reduces available RAM
// If neither is defined then data is not buffered at all
//    Pro: fast, does not occupy any RAM
//    Con: text that runs over image will not be removed until a new background 
//         image is shown => need to display text outside of IMAGE_REGION
//#define IMGDATA_FILEBUF     "$BGIMAGE.BIN$"
//#define IMGDATA_MEMBUF      4096

// Define region in which (background) image will be shown
#if defined(IMGDATA_FILEBUF) || defined(IMGDATA_MEMBUF)
#define IMAGE_REGION_X      0
#define IMAGE_REGION_Y      0
#define IMAGE_REGION_WIDTH  240
#define IMAGE_REGION_HEIGHT 240
#else
#define IMAGE_REGION_X      0
#define IMAGE_REGION_Y      51
#define IMAGE_REGION_WIDTH  240
#define IMAGE_REGION_HEIGHT 160
#endif

// this is the background image shown when NO disk image (.Dxx/.Gxx) is mounted
#define DEFAULT_IMAGE       "DEFAULT.GIF"

// this is the background image shown when a disk image is mounted for which no individual image is found
#define DEFAULT_DISK_IMAGE  "DISK.GIF"

// derive our own ST7789 class so we can access parameters that are 
// not available otherwise (e.g. the SPI mode)
class Arduino_ST7789m : public Arduino_ST7789
{
public:
  Arduino_ST7789m(Arduino_DataBus *bus, int8_t rst = GFX_NOT_DEFINED);
  virtual ~Arduino_ST7789m();
  
  bool begin(int32_t speed, int8_t spiMode);

  void clearDisplay();
  void clearCurrentLine();
  void clearRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

  int  getTextLineHeight();

  uint32_t startImage(int16_t x, int16_t y, uint16_t w, uint16_t h);
  void     addImageData(uint16_t *data, uint32_t npixels);
  void     endImage();

  uint32_t getBufferSize() { return 2048; }
  uint8_t *getBuffer();

private:
#if defined(IMGDATA_FILEBUF)
  File     m_imgDataFile;
#elif defined(IMGDATA_MEMBUF)
#define IMGDATA_MAX_CHUNKS (((IMAGE_REGION_WIDTH)*(IMAGE_REGION_HEIGHT)*2)/(IMGDATA_MEMBUF)+1)
  uint8_t *m_imgDataChunk[IMGDATA_MAX_CHUNKS];
  uint32_t m_imgDataPos;
#else
  uint32_t m_imgDataPos;
#endif  

  int32_t  m_imgX, m_imgY;
  uint32_t m_imgW, m_imgH;
  int32_t  m_pixRowStart, m_pixRowEnd, m_pixCurrent;
  int32_t  m_pixColStart, m_pixColEnd, m_pixWidth;
  uint8_t *m_buffer;
  void     (Arduino_ST7789m::*doAddImageData)(uint16_t *data, uint32_t npixels);

  void saveImageData(uint8_t *data, uint32_t size);
  void sendImageData(uint32_t pos, uint32_t size);

  void addImageDataNoClip(uint16_t *data, uint32_t npixels);
  void addImageDataClipY(uint16_t *data, uint32_t npixels);
  void addImageDataClipXY(uint16_t *data, uint32_t npixels);
  void drawBackground(int32_t x, int32_t y, uint32_t w, uint32_t h, uint16_t bgcolor);
};


Arduino_ST7789m::Arduino_ST7789m(Arduino_DataBus *bus, int8_t rst) : 
  Arduino_ST7789(bus, rst, 0, false, 240, 240, 0, 0, 0, 80)
{ 
  m_imgY = 0; 
  m_imgH = 0; 
  m_buffer = NULL; 

#ifdef IMGDATA_MEMBUF
  for(int i=0; i<IMGDATA_MAX_CHUNKS; i++) m_imgDataChunk[i] = NULL;
  m_imgDataPos = 0;
#elif !defined(IMGDATA_FILEBUF)
  m_imgDataPos = 0;
#endif
}

Arduino_ST7789m::~Arduino_ST7789m()
{ 
#if defined(IMGDATA_FILEBUF)
  m_imgDataFile.close(); 
#elif defined(IMGDATA_MEMBUF)
  for(int i=0; i<IMGDATA_MAX_CHUNKS && m_imgDataChunk[i]!=NULL; i++)
    free(m_imgDataChunk[i]);
#endif
  delete [] m_buffer; 
}


bool Arduino_ST7789m::begin(int32_t speed, int8_t spiMode)
{ 
  _override_datamode = spiMode;

#if defined(IMGDATA_FILEBUF) || defined(IMGDATA_MEMBUF)
  return Arduino_TFT::begin(speed);
#else
  if( Arduino_TFT::begin(speed) )
    { fillRect(0, 0, width(), height(), RGB565_BLACK); return true; }
  else
    return false;
#endif
}


uint8_t *Arduino_ST7789m::getBuffer() 
{ 
  if( m_buffer==NULL ) m_buffer = new uint8_t[getBufferSize()];
  return m_buffer;
}


void Arduino_ST7789m::clearDisplay()
{ 
  clearRegion(0, 0, width(), height()); 
}

void Arduino_ST7789m::clearCurrentLine()
{ 
  clearRegion(0, cursor_y, width(), getTextLineHeight()); 
}

void Arduino_ST7789m::clearRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{ 
  drawBackground(x, y, w, h, RGB565_BLACK); 
}

int Arduino_ST7789m::getTextLineHeight()
{ 
  return (gfxFont==NULL ? 8 : gfxFont->yAdvance) * textsize_y; 
}

uint32_t Arduino_ST7789m::startImage(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
  doAddImageData = NULL;

  // center image
  if( x==0x7FFF ) x = ((IMAGE_REGION_WIDTH)-w)/2;
  if( y==0x7FFF ) y = ((IMAGE_REGION_HEIGHT)-h)/2;

  // clip image coordinates, x and y are relative to image region,
  // m_imgX and m_imgY are full-screen coordinates
  m_imgX = (IMAGE_REGION_X) + max(x, 0);
  m_imgY = (IMAGE_REGION_Y) + max(y, 0);
  m_imgW = max(min(x+w, IMAGE_REGION_WIDTH)  - max(x, 0), 0);
  m_imgH = max(min(y+h, IMAGE_REGION_HEIGHT) - max(y, 0), 0);

#if defined(IMGDATA_FILEBUF)
  m_imgDataFile.close();
  m_imgDataFile = LittleFS.open(IMGDATA_FILEBUF, "w");
  if( !m_imgDataFile ) return 0;
#elif defined(IMGDATA_MEMBUF)
  // free memory from previous image
  for(int i=0; i<IMGDATA_MAX_CHUNKS && m_imgDataChunk[i]!=NULL; i++)
    { free(m_imgDataChunk[i]); m_imgDataChunk[i] = NULL; }

  // allocate memory for this image
  uint32_t s = m_imgW * m_imgH * 2, chunk = 0;
  while( s>0 )
    {
      uint32_t n = min(s, IMGDATA_MEMBUF);
      m_imgDataChunk[chunk] = (uint8_t *) calloc(1, n);
      if( m_imgDataChunk[chunk]==NULL ) break;
      s -= n;
      chunk++;
    }
  
  if( s>0 )
    {
      // we were unable to allocate all required memory, release any memory we did allocate
      for(int i=0; i<chunk; i++) { free(m_imgDataChunk[i]); m_imgDataChunk[i] = NULL; }
      return 0;
    }
  m_imgDataPos = 0;
#else
  startWrite();
  m_imgDataPos = 0;
  writeAddrWindow(m_imgX, m_imgY, m_imgW, m_imgH);
#endif

  // clipping parameters
  m_pixCurrent  = 0;
  m_pixRowStart = (m_imgY - (IMAGE_REGION_Y) - y) * w;
  m_pixRowEnd   = m_pixRowStart + m_imgH * w;

  if( x>=IMAGE_REGION_X && x+w<=(IMAGE_REGION_X)+(IMAGE_REGION_WIDTH) )
    {
      // image is completely within X boundaries
      if( y>=IMAGE_REGION_Y && y+h<=(IMAGE_REGION_Y)+(IMAGE_REGION_HEIGHT) )
        {
          // image is completely within X and Y boundaries => no clipping necessary
          doAddImageData = &Arduino_ST7789m::addImageDataNoClip;
        }
      else if( y+h>=IMAGE_REGION_Y && y<(IMAGE_REGION_Y)+(IMAGE_REGION_HEIGHT) )
        {
          // image is partially within Y boundaries => only clip Y
          doAddImageData = &Arduino_ST7789m::addImageDataClipY;
        }
    }
  else if( x+w>=IMAGE_REGION_X && x<IMAGE_REGION_WIDTH )
    {
      // image is partially within X and Y boundaries => clip X and Y
      m_pixColStart  = m_imgX - (IMAGE_REGION_X) - x;
      m_pixColEnd    = m_pixColStart + m_imgW;
      m_pixWidth     = w;
      doAddImageData = &Arduino_ST7789m::addImageDataClipXY;
    }

  return 2; // bytes per pixel
}


void Arduino_ST7789m::addImageData(uint16_t *data, uint32_t npixels)
{
  if( doAddImageData!=NULL ) (this->*doAddImageData)(data, npixels);
}


void Arduino_ST7789m::endImage()
{
  // in case that for whatever reason (e.g. transmission error) 
  // we did not receive all expected data
#if defined(IMGDATA_FILEBUF)
  if( m_imgDataFile && m_imgDataFile.size() < (m_imgW*m_imgH*2) )
    {
      uint32_t bytesRemaining = (m_imgW*m_imgH*2) - m_imgDataFile.size();
      uint8_t *buffer = getBuffer();
      memset(buffer, 0, getBufferSize());
      while( bytesRemaining>0 )
        {
          uint32_t n = min(bytesRemaining, getBufferSize());
          m_imgDataFile.write(buffer, n);
          bytesRemaining-=n;
        }
    }

  m_imgDataFile.close();
  m_imgDataFile = LittleFS.open(IMGDATA_FILEBUF, "r");
#elif !defined(IMGDATA_MEMBUF)
  if( m_imgDataPos < m_imgW*m_imgH*2 )
    writeRepeat(RGB565_BLACK, (m_imgW*m_imgH)-m_imgDataPos/2);
  endWrite();
#endif
}


void Arduino_ST7789m::addImageDataNoClip(uint16_t *data, uint32_t npixels)
{
  saveImageData((uint8_t *) data, npixels*2);
}


void Arduino_ST7789m::addImageDataClipY(uint16_t *data, uint32_t npixels)
{
  if( m_pixCurrent < m_pixRowStart )
    {
      // current data starts before Y boundary
      int32_t offset = min(m_pixRowStart-m_pixCurrent, npixels);
      if( offset < npixels )
        saveImageData((uint8_t *) (data+offset), (npixels-offset)*2);
    }
  else if( m_pixCurrent < m_pixRowEnd )
    {
      // current data starts within Y boundary
      int32_t overlap = min(m_pixRowEnd - m_pixCurrent, npixels);
      saveImageData((uint8_t *) data, overlap*2);
    };

  m_pixCurrent += npixels;
}


void Arduino_ST7789m::addImageDataClipXY(uint16_t *data, uint32_t npixels)
{ 
  // this function allows for X and Y clipping, it could be used universally
  // but using dedicated functions for the simpler (and more common) cases is faster
  if( m_pixCurrent < m_pixRowStart )
    {
      // current data starts before Y boundary => skip
      int32_t offset = min(m_pixRowStart-m_pixCurrent, npixels);
      m_pixCurrent += offset;
      data         += offset;
      npixels      -= offset;
    }

  while( npixels>0 && m_pixCurrent<m_pixRowEnd )
    {
      // we have data within the Y boundary
      int32_t pixColCurrent = m_pixCurrent % m_pixWidth;

      if( pixColCurrent < m_pixColStart )
        {
          // current data starts before X boundary => skip
          int32_t offset = min(m_pixColStart-pixColCurrent, npixels);
          m_pixCurrent  += offset;
          pixColCurrent += offset;
          data          += offset;
          npixels       -= offset;
        }

      if( (pixColCurrent < m_pixColEnd) && npixels>0 )
        {
          // write overlapping data
          int32_t overlap = min(m_pixColEnd-pixColCurrent, npixels);
          saveImageData((uint8_t *) data, overlap*2);
          m_pixCurrent  += overlap;
          pixColCurrent += overlap;
          data          += overlap;
          npixels       -= overlap;
        }

      if( pixColCurrent < m_pixWidth )
        {
          // skip remaining data at end of row
          int32_t rest = min(m_pixWidth-pixColCurrent, npixels);
          m_pixCurrent  += rest;
          data          += rest;
          npixels       -= rest;
        }
    }
}


void Arduino_ST7789m::drawBackground(int32_t x, int32_t y, uint32_t w, uint32_t h, uint16_t bgcolor)
{
  // clear region above background image
  if( y<m_imgY )
    {
      uint32_t n = min(m_imgY-y, h);
      fillRect(x, y, w, n, bgcolor);
      y += n;
      h -= n;
    }

  // clear region left of background image
  if( m_imgX>x ) fillRect(x, y, m_imgX-x, min(m_imgH, h), bgcolor);

  // clear region right of background image
  if( m_imgX+m_imgW<x+w ) fillRect(m_imgX+m_imgW, y, x+w-m_imgX-m_imgW, min(m_imgH, h), bgcolor);

#if !defined(IMGDATA_FILEBUF) && !defined(IMGDATA_MEMBUF)
  // background buffering is disabled => skip image region
  if( y<m_imgY+m_imgH )
    {
      uint32_t ih = min(h, m_imgY+m_imgH-y);
      y += ih;
      h -= ih;
    }
#else
  // only copy image data if drawing region intersects the image
#ifdef IMGDATA_FILEBUF
  if( m_imgDataFile && y<m_imgY+m_imgH && h>0 )
#else
  if( m_imgDataChunk[0]!=NULL && y<m_imgY+m_imgH && h>0 )
#endif
    {
      // send background image data to display
      // only copy image data if the drawing region intersects the image
      uint32_t ih = min(h, m_imgY+m_imgH-y);
      int32_t  xs = max(x, m_imgX), xe = min(x+w, m_imgX+m_imgW), xw = xe-xs;
      if( xw>0 )
        {
          startWrite();
          writeAddrWindow(xs, y, xw, ih);
          if( xw == m_imgW )
            {
              // drawing region width is the full image => can copy multiple rows at once
              sendImageData(((y-m_imgY)*m_imgW)*2, ih*m_imgW*2);
              h -= ih;
              y += ih;
            }
          else
            {
              // drawing region width is less than image width => must copy one row at a time
              while( ih>0 )
                {
                  sendImageData(((y-m_imgY)*m_imgW+xs-m_imgX)*2, xw*2);
                  y++;
                  h--;
                  ih--;
                }
            }
          endWrite();
        }
    }
#endif

  // clear region below background image
  if( h>0 ) fillRect(x, y, w, h, bgcolor);
}


void Arduino_ST7789m::saveImageData(uint8_t *data, uint32_t size)
{
#if defined(IMGDATA_FILEBUF)
  m_imgDataFile.write(data, size);
#elif defined(IMGDATA_MEMBUF)
  uint32_t chunk  = m_imgDataPos / (IMGDATA_MEMBUF);
  uint32_t offset = m_imgDataPos % (IMGDATA_MEMBUF);
  while( size>0 )
    {
      uint32_t n = min(size, (IMGDATA_MEMBUF)-offset);
      memcpy(m_imgDataChunk[chunk]+offset, data, n);
      m_imgDataPos += n;
      data += n;
      size -= n;
      offset = 0;
      chunk++;
    }
#else
  // not buffering => send data directly to display
  _bus->writePixels((uint16_t *) data, size/2);
  m_imgDataPos += size;
#endif
}


void Arduino_ST7789m::sendImageData(uint32_t pos, uint32_t size)
{
#if defined(IMGDATA_FILEBUF)
  uint16_t *buffer    = (uint16_t *) getBuffer();
  uint32_t bufferSize = getBufferSize();
  m_imgDataFile.seek(pos, SeekSet);
  while( size>0 )
    {
      uint32_t n = min(size, bufferSize);
      m_imgDataFile.read((uint8_t *) buffer, n);
      _bus->writePixels(buffer, n/2);
     size -= n;
    }
#elif defined(IMGDATA_MEMBUF)
  uint32_t chunk  = pos / (IMGDATA_MEMBUF);
  uint32_t offset = pos % (IMGDATA_MEMBUF);
  while( size>0 )
    {
      uint32_t n = min(size, (IMGDATA_MEMBUF)-offset);
      _bus->writePixels((uint16_t *) (m_imgDataChunk[chunk]+offset), n/2);
      size -= n;
      offset = 0;
      chunk++;
    }
#endif
}


// ----------------------------------------------------------------------------------------------------------------

static void   *gifOpen(const char *filename, int32_t *size);
static void    gifClose(void *handle);
static int32_t gifSeek(GIFFILE *handle, int32_t iPosition);
static int32_t gifRead(GIFFILE *handle, uint8_t *buffer, int32_t length);
static void    gifDraw(GIFDRAW *pDraw);


IECDisplay_ST7789::IECDisplay_ST7789()
{
  Arduino_DataBus *bus = new Arduino_HWSPI(PIN_ST7789_DC, GFX_NOT_DEFINED, &PIN_ST7789_SPI);
  gpio_set_function(PIN_ST7789_SPI_SCL,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_ST7789_SPI_COPI, GPIO_FUNC_SPI);

  m_display = new Arduino_ST7789m(bus, PIN_ST7789_RES);
}


IECDisplay_ST7789::~IECDisplay_ST7789()
{
  delete m_display;
}


void IECDisplay_ST7789::begin(uint32_t rotation)
{
  m_display->begin(GFX_NOT_DEFINED /* SPI speed */, SPI_MODE3);

  m_display->invertDisplay(true);
  m_display->setRotation((rotation/90)%4);
  m_display->setTextWrap(false);
  m_display->setTextColor(RGB565_WHITE);
  setBackgroundImage(DEFAULT_IMAGE, false);
  m_display->clearDisplay();
}


void IECDisplay_ST7789::showMessage(std::string msg)
{
  // display "Searching..." message while finding disk image if button is pressed
  // this can rewrite any part of the display, "redraw" will automatically be called
  // after the operation finishes
  m_display->setTextSize(3);
  m_display->setCursor(0, 1*m_display->getTextLineHeight());
  m_display->setTextColor(RGB565_WHITE);
  m_display->print(msg.c_str());
}


void IECDisplay_ST7789::showTransmitMessage(std::string msg, std::string fileName)
{
  // display "Sendingg/Receiving" message while transferring files over USB
  // this can rewrite any part of the display, "redraw" will automatically be called
  // after the operation finishes
  m_display->clearDisplay();
  m_display->setCursor(0,0);
  m_display->setTextSize(3);
  m_display->setTextColor(RGB565_YELLOW);
  m_display->println(msg.c_str());
  m_display->setTextColor(RGB565_WHITE);
  m_display->print(fileName.c_str());
}


void IECDisplay_ST7789::dispStatus(const string &s, bool clear)
{
  // display drive status (e.g. "00, OK, 00, 00")
  int color = RGB565_WHITE;
  if( isdigit(s[0]) && isdigit(s[1]) )
    {
      int code = (s[0]-'0')*10 + (s[1]-'0');
      color = code < 20 ? RGB565_GREEN : RGB565_RED;
    }
  
  m_display->setTextSize(2);
  m_display->setCursor(0, m_display->height()-m_display->getTextLineHeight()-10);
  m_display->clearCurrentLine();
  m_display->setTextColor(color);
  m_display->print(s.c_str());
}


void IECDisplay_ST7789::dispImageName(const string &s, bool clear)
{
  // display name of currently mounted disk image
  m_display->setTextSize(3);
  m_display->setTextColor(RGB565_YELLOW);
  m_display->setCursor(0, 0*m_display->getTextLineHeight());
  if( clear ) m_display->clearCurrentLine();
  m_display->println(s.c_str());
}


void IECDisplay_ST7789::dispFileName(const string &s, bool clear)
{
  // display name of file currently being loaded or saved
  m_display->setTextSize(3);
  m_display->setCursor(0, m_display->getTextLineHeight()+4);
  m_display->setFont(&cbmFont);
  m_display->setTextSize(2);
  if( clear ) m_display->clearCurrentLine();
  m_display->setTextColor(RGB565_WHITE);
  m_display->print(s.c_str());
  m_display->setFont(NULL);
}


void IECDisplay_ST7789::setCurrentImageName(std::string iname)
{
  IECDisplay::setCurrentImageName(iname);

  if( iname.empty() )
    setBackgroundImage(DEFAULT_IMAGE);
  else
    {
      // find .GIF file for disk image (.Dxx)
      bool found = false;
      size_t dot = iname.rfind('.');
      if( dot!=string::npos ) found = setBackgroundImage(iname.substr(0,dot)+".GIF", false);
      if( !found ) setBackgroundImage(DEFAULT_DISK_IMAGE, false);
      redraw();
    }
}

void IECDisplay_ST7789::setCurrentFileName(std::string fname)
{
  IECDisplay::setCurrentFileName(fname);
  dispFileName(fname);
}


void IECDisplay_ST7789::setStatusMessage(std::string msg)
{
  IECDisplay::setStatusMessage(msg);
  dispStatus(msg);
}


void IECDisplay_ST7789::startProgress(int nbytestotal)
{
  IECDisplay::startProgress(nbytestotal);
  m_display->fillRect(0, m_display->height()-5, m_display->width(), 5, RGB565_BLACK);
}


void IECDisplay_ST7789::updateProgress(int nbytes)
{
  m_curFileBytesRead += nbytes;

  if( m_curFileSize>0 )
    {
      int w = (m_display->width() * m_curFileBytesRead) / m_curFileSize;
      if( (w-m_progressWidth)>0 )
        {
          m_display->fillRect(m_progressWidth, m_display->height()-5, w-m_progressWidth, 5, RGB565_WHITE);
          m_progressWidth = w;
        }
    }
  else
    {
      const int markerSize = 16;
      int width = m_display->width()-markerSize;
      int np = ((width*2) * (m_curFileBytesRead & 4095)) / 4096;
      int pp = m_progressWidth;
      if( np != pp )
        {
          m_progressWidth = np;
          if( np >= width ) np = width*2-1-np;
          if( pp >= width ) pp = width*2-1-pp;

          // draw marker
          m_display->fillRect(np, m_display->height()-5, markerSize, 5, RGB565_WHITE);

          // erase pixels before/after marker
          if( np>pp )
            m_display->fillRect(pp, m_display->height()-5, np-pp, 5, RGB565_BLACK);
          else if( np<pp )
            m_display->fillRect(np+markerSize, m_display->height()-5, pp-np, 5, RGB565_BLACK);
        }
    }
}


void IECDisplay_ST7789::endProgress()
{
  m_display->clearRegion(0, m_display->height()-5, m_display->width(), 5);
}


void IECDisplay_ST7789::redraw()
{
  m_display->clearDisplay();
  dispImageName(m_curImageName.empty() ? "<SD>" : m_curImageName.c_str(), false);
  dispFileName(m_curFileName, false);
  dispStatus(m_statusMessage, false);
}


void IECDisplay_ST7789::showPrintStatus(bool printing)
{
  static int spin = 0;
  static unsigned long spinto = 0;
  const char spinchr[] = "|/-\\";

  if( !printing || millis()>spinto )
    {
      m_display->setTextSize(3);
      int w = 25;
      int h = m_display->getTextLineHeight();
      int x = m_display->width()-w;
      int y = h;

      m_display->clearRegion(x, y, w, y+h);
      if( printing )
        {
          m_display->setTextColor(RGB565_YELLOW);
          m_display->setCursor(x, y);
          m_display->write(spinchr[spin]);
          spin   = (spin+1) & 3;
          spinto = millis()+250;
        }
    }
}


uint32_t IECDisplay_ST7789::startImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  return m_display->startImage(x, y, w, h);
}


void IECDisplay_ST7789::addImageData(uint8_t *data, uint32_t dataSize)
{
  m_display->addImageData((uint16_t *) data, dataSize/2);
}


void IECDisplay_ST7789::endImage()
{
  m_display->endImage();
}


bool IECDisplay_ST7789::setBackgroundImageGIF(uint8_t *data, uint32_t size, int32_t x, int32_t y, bool doUpdate)
{
  bool res = false;

  AnimatedGIF *gif = new AnimatedGIF();
  if( gif!=NULL )
    {
      if( gif->open(data, size, gifDraw) )
        {
          startImage(x, y, gif->getCanvasWidth(), gif->getCanvasHeight());
          gif->playFrame(true, NULL, m_display);
          gif->close();
          endImage();
          res = true;
        }
      
      delete gif;
    }
  
  // if requested then update the actual display too
  if( res && doUpdate ) redraw();
  
  return res;
}


bool IECDisplay_ST7789::setBackgroundImage(const string &fname, bool doUpdate)
{
  bool res = false;

  // get extension (uppercase)
  string ext;
  size_t dot = fname.rfind('.');
  if( dot!=string::npos ) ext = fname.substr(dot+1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){return std::toupper(c);});

  // if fname does not exist but a hidden version ($fname$) exists then use that
  string filename;
  if( LittleFS.exists(fname.c_str()) )
    filename = fname;
  else if( LittleFS.exists(("$"+fname+"$").c_str()) )
    filename = "$"+fname+"$";

  // open file depending on extension
  if( ext=="GIF" )
    res = setBackgroundImageGIF(filename);
  else if( ext=="BIN" )
    res = setBackgroundImageRGB(filename);

  // if requested then update the actual display too
  if( doUpdate ) redraw();

  return res;
}


bool IECDisplay_ST7789::setBackgroundImageRGB(const string &filename)
{
  bool res = false;

  int w = m_display->width(), h = m_display->height();
  size_t i = filename.rfind('_');
  if( i!=string::npos )
    sscanf(filename.substr(i).c_str(), "_%iX%i", &w, &h);
  
  File f = LittleFS.open(filename.c_str(), "r");
  if( f )
    {
      uint32_t n = startImage(0x7FFF, 0x7FFF, w, h);
      if( n>0 )
        {
          uint32_t bufSize = m_display->getBufferSize();
          uint8_t *buffer = m_display->getBuffer();
          while( (n=f.read(buffer, bufSize))>0 ) addImageData(buffer, n);
          endImage();
          res = true;
        }
    }
  
  return res;
}


bool IECDisplay_ST7789::setBackgroundImageGIF(const string &fname)
{
  bool res = false;

  AnimatedGIF *gif = new AnimatedGIF();
  if( gif!=NULL )
    {
      if( gif->open(fname.c_str(), gifOpen, gifClose, gifRead, gifSeek, gifDraw) )
        {
          startImage(0x7FFF, 0x7FFF, gif->getCanvasWidth(), gif->getCanvasHeight());
          gif->playFrame(true, NULL, m_display);
          gif->close();
          endImage();
          res = true;
        }

      delete gif;
    }
  
  return res;
}


// --------------------- callback functions for GIF decoding -------------------


static void *gifOpen(const char *filename, int32_t *size) 
{
  static File myfile;
  myfile = LittleFS.open(filename, "r");
  if (myfile)
    {
      *size = myfile.size();
      return &myfile;
    }

  return NULL;
}


static void gifClose(void *handle) 
{
  File *pFile = (File *)handle;
  if (pFile) pFile->close();
}


static int32_t gifRead(GIFFILE *handle, uint8_t *buffer, int32_t length) 
{
  int32_t iBytesRead;
  iBytesRead = length;
  File *f = static_cast<File *>(handle->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((handle->iSize - handle->iPos) < length)
    iBytesRead = handle->iSize - handle->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t) f->read(buffer, iBytesRead);
  handle->iPos = f->position();

  return iBytesRead;
}


static int32_t gifSeek(GIFFILE *handle, int32_t iPosition) 
{
  File *f = static_cast<File *>(handle->fHandle);
  f->seek(iPosition);
  handle->iPos = (int32_t)f->position();

  return handle->iPos;
}


static void gifDraw(GIFDRAW *pDraw)
{
  Arduino_ST7789m *display = (Arduino_ST7789m *) pDraw->pUser;
  uint16_t buffer[1024];
  uint32_t n = min(pDraw->iWidth, 1024);
  for(int i=0; i<n; i++) buffer[i] = pDraw->pPalette[pDraw->pPixels[i]];
  display->addImageData(buffer, pDraw->iWidth);
}


// ---------------------------- CBM font -----------------------------------


/* CBM screen font (character set 1: uppercase text and graphics) */
static const uint8_t cbmFontData[128*8] = {
  0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00,  // #0
  0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00,  // #1
  0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00,  // #2
  0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00,  // #3
  0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00,  // #4
  0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00,  // #5
  0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00,  // #6
  0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00,  // #7
  0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00,  // #8
  0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00,  // #9
  0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00,  // #10
  0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00,  // #11
  0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00,  // #12
  0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00,  // #13
  0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00,  // #14
  0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00,  // #15
  0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00,  // #16
  0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00,  // #17
  0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00,  // #18
  0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00,  // #19
  0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,  // #20
  0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00,  // #21
  0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00,  // #22
  0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00,  // #23
  0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00,  // #24
  0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00,  // #25
  0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00,  // #26
  0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00,  // #27
  0x0C, 0x12, 0x30, 0x7C, 0x30, 0x62, 0xFC, 0x00,  // #28
  0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00,  // #29
  0x00, 0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18,  // #30
  0x00, 0x10, 0x30, 0x7F, 0x7F, 0x30, 0x10, 0x00,  // #31
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // #32
  0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00,  // #33
  0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,  // #34
  0x66, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0x66, 0x00,  // #35
  0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00,  // #36
  0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00,  // #37
  0x3C, 0x66, 0x3C, 0x38, 0x67, 0x66, 0x3F, 0x00,  // #38
  0x06, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,  // #39
  0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00,  // #40
  0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00,  // #41
  0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00,  // #42
  0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00,  // #43
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30,  // #44
  0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00,  // #45
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,  // #46
  0x00, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00,  // #47
  0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00,  // #48
  0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E, 0x00,  // #49
  0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00,  // #50
  0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00,  // #51
  0x06, 0x0E, 0x1E, 0x66, 0x7F, 0x06, 0x06, 0x00,  // #52
  0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00,  // #53
  0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00,  // #54
  0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00,  // #55
  0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00,  // #56
  0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00,  // #57
  0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00,  // #58
  0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30,  // #59
  0x0E, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0E, 0x00,  // #60
  0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00,  // #61
  0x70, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x70, 0x00,  // #62
  0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00,  // #63
  0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00,  // #64
  0x08, 0x1C, 0x3E, 0x7F, 0x7F, 0x1C, 0x3E, 0x00,  // #65
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,  // #66
  0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00,  // #67
  0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,  // #68
  0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,  // #69
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,  // #70
  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,  // #71
  0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,  // #72
  0x00, 0x00, 0x00, 0xE0, 0xF0, 0x38, 0x18, 0x18,  // #73
  0x18, 0x18, 0x1C, 0x0F, 0x07, 0x00, 0x00, 0x00,  // #74
  0x18, 0x18, 0x38, 0xF0, 0xE0, 0x00, 0x00, 0x00,  // #75
  0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF,  // #76
  0xC0, 0xE0, 0x70, 0x38, 0x1C, 0x0E, 0x07, 0x03,  // #77
  0x03, 0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xC0,  // #78
  0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,  // #79
  0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,  // #80
  0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00,  // #81
  0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00,  // #82
  0x36, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C, 0x08, 0x00,  // #83
  0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,  // #84
  0x00, 0x00, 0x00, 0x07, 0x0F, 0x1C, 0x18, 0x18,  // #85
  0xC3, 0xE7, 0x7E, 0x3C, 0x3C, 0x7E, 0xE7, 0xC3,  // #86
  0x00, 0x3C, 0x7E, 0x66, 0x66, 0x7E, 0x3C, 0x00,  // #87
  0x18, 0x18, 0x66, 0x66, 0x18, 0x18, 0x3C, 0x00,  // #88
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,  // #89
  0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08, 0x00,  // #90
  0x18, 0x18, 0x18, 0xFF, 0xFF, 0x18, 0x18, 0x18,  // #91
  0xC0, 0xC0, 0x30, 0x30, 0xC0, 0xC0, 0x30, 0x30,  // #92
  0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,  // #93
  0x00, 0x00, 0x03, 0x3E, 0x76, 0x36, 0x36, 0x00,  // #94
  0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01,  // #95
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // #96
  0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,  // #97
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,  // #98
  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // #99
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,  // #100
  0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,  // #101
  0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33,  // #102
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,  // #103
  0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0x33, 0x33,  // #104
  0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80,  // #105
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,  // #106
  0x18, 0x18, 0x18, 0x1F, 0x1F, 0x18, 0x18, 0x18,  // #107
  0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F,  // #108
  0x18, 0x18, 0x18, 0x1F, 0x1F, 0x00, 0x00, 0x00,  // #109
  0x00, 0x00, 0x00, 0xF8, 0xF8, 0x18, 0x18, 0x18,  // #110
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,  // #111
  0x00, 0x00, 0x00, 0x1F, 0x1F, 0x18, 0x18, 0x18,  // #112
  0x18, 0x18, 0x18, 0xFF, 0xFF, 0x00, 0x00, 0x00,  // #113
  0x00, 0x00, 0x00, 0xFF, 0xFF, 0x18, 0x18, 0x18,  // #114
  0x18, 0x18, 0x18, 0xF8, 0xF8, 0x18, 0x18, 0x18,  // #115
  0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,  // #116
  0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0,  // #117
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,  // #118
  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // #119
  0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,  // #120
  0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,  // #121
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF,  // #122
  0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0,  // #123
  0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00,  // #124
  0x18, 0x18, 0x18, 0xF8, 0xF8, 0x00, 0x00, 0x00,  // #125
  0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00,  // #126
  0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,  // #127
};

// mapping PETSCII character codes to screen font character set codes 
// (https://sta.c64.org/cbm64pettoscr.html)
static const GFXglyph cbmFontGlyph[256] = {
  {8*128, 8, 8, 8, 0, 0}, // #0
  {8*129, 8, 8, 8, 0, 0}, // #1
  {8*130, 8, 8, 8, 0, 0}, // #2
  {8*131, 8, 8, 8, 0, 0}, // #3
  {8*132, 8, 8, 8, 0, 0}, // #4
  {8*133, 8, 8, 8, 0, 0}, // #5
  {8*134, 8, 8, 8, 0, 0}, // #6
  {8*135, 8, 8, 8, 0, 0}, // #7
  {8*136, 8, 8, 8, 0, 0}, // #8
  {8*137, 8, 8, 8, 0, 0}, // #9
  {8*138, 8, 8, 8, 0, 0}, // #10
  {8*139, 8, 8, 8, 0, 0}, // #11
  {8*140, 8, 8, 8, 0, 0}, // #12
  {8*141, 8, 8, 8, 0, 0}, // #13
  {8*142, 8, 8, 8, 0, 0}, // #14
  {8*143, 8, 8, 8, 0, 0}, // #15
  {8*144, 8, 8, 8, 0, 0}, // #16
  {8*145, 8, 8, 8, 0, 0}, // #17
  {8*146, 8, 8, 8, 0, 0}, // #18
  {8*147, 8, 8, 8, 0, 0}, // #19
  {8*148, 8, 8, 8, 0, 0}, // #20
  {8*149, 8, 8, 8, 0, 0}, // #21
  {8*150, 8, 8, 8, 0, 0}, // #22
  {8*151, 8, 8, 8, 0, 0}, // #23
  {8*152, 8, 8, 8, 0, 0}, // #24
  {8*153, 8, 8, 8, 0, 0}, // #25
  {8*154, 8, 8, 8, 0, 0}, // #26
  {8*155, 8, 8, 8, 0, 0}, // #27
  {8*156, 8, 8, 8, 0, 0}, // #28
  {8*157, 8, 8, 8, 0, 0}, // #29
  {8*158, 8, 8, 8, 0, 0}, // #30
  {8*159, 8, 8, 8, 0, 0}, // #31
  {8* 32, 8, 8, 8, 0, 0}, // #32
  {8* 33, 8, 8, 8, 0, 0}, // #33
  {8* 34, 8, 8, 8, 0, 0}, // #34
  {8* 35, 8, 8, 8, 0, 0}, // #35
  {8* 36, 8, 8, 8, 0, 0}, // #36
  {8* 37, 8, 8, 8, 0, 0}, // #37
  {8* 38, 8, 8, 8, 0, 0}, // #38
  {8* 39, 8, 8, 8, 0, 0}, // #39
  {8* 40, 8, 8, 8, 0, 0}, // #40
  {8* 41, 8, 8, 8, 0, 0}, // #41
  {8* 42, 8, 8, 8, 0, 0}, // #42
  {8* 43, 8, 8, 8, 0, 0}, // #43
  {8* 44, 8, 8, 8, 0, 0}, // #44
  {8* 45, 8, 8, 8, 0, 0}, // #45
  {8* 46, 8, 8, 8, 0, 0}, // #46
  {8* 47, 8, 8, 8, 0, 0}, // #47
  {8* 48, 8, 8, 8, 0, 0}, // #48
  {8* 49, 8, 8, 8, 0, 0}, // #49
  {8* 50, 8, 8, 8, 0, 0}, // #50
  {8* 51, 8, 8, 8, 0, 0}, // #51
  {8* 52, 8, 8, 8, 0, 0}, // #52
  {8* 53, 8, 8, 8, 0, 0}, // #53
  {8* 54, 8, 8, 8, 0, 0}, // #54
  {8* 55, 8, 8, 8, 0, 0}, // #55
  {8* 56, 8, 8, 8, 0, 0}, // #56
  {8* 57, 8, 8, 8, 0, 0}, // #57
  {8* 58, 8, 8, 8, 0, 0}, // #58
  {8* 59, 8, 8, 8, 0, 0}, // #59
  {8* 60, 8, 8, 8, 0, 0}, // #60
  {8* 61, 8, 8, 8, 0, 0}, // #61
  {8* 62, 8, 8, 8, 0, 0}, // #62
  {8* 63, 8, 8, 8, 0, 0}, // #63
  {8*  0, 8, 8, 8, 0, 0}, // #64
  {8*  1, 8, 8, 8, 0, 0}, // #65
  {8*  2, 8, 8, 8, 0, 0}, // #66
  {8*  3, 8, 8, 8, 0, 0}, // #67
  {8*  4, 8, 8, 8, 0, 0}, // #68
  {8*  5, 8, 8, 8, 0, 0}, // #69
  {8*  6, 8, 8, 8, 0, 0}, // #70
  {8*  7, 8, 8, 8, 0, 0}, // #71
  {8*  8, 8, 8, 8, 0, 0}, // #72
  {8*  9, 8, 8, 8, 0, 0}, // #73
  {8* 10, 8, 8, 8, 0, 0}, // #74
  {8* 11, 8, 8, 8, 0, 0}, // #75
  {8* 12, 8, 8, 8, 0, 0}, // #76
  {8* 13, 8, 8, 8, 0, 0}, // #77
  {8* 14, 8, 8, 8, 0, 0}, // #78
  {8* 15, 8, 8, 8, 0, 0}, // #79
  {8* 16, 8, 8, 8, 0, 0}, // #80
  {8* 17, 8, 8, 8, 0, 0}, // #81
  {8* 18, 8, 8, 8, 0, 0}, // #82
  {8* 19, 8, 8, 8, 0, 0}, // #83
  {8* 20, 8, 8, 8, 0, 0}, // #84
  {8* 21, 8, 8, 8, 0, 0}, // #85
  {8* 22, 8, 8, 8, 0, 0}, // #86
  {8* 23, 8, 8, 8, 0, 0}, // #87
  {8* 24, 8, 8, 8, 0, 0}, // #88
  {8* 25, 8, 8, 8, 0, 0}, // #89
  {8* 26, 8, 8, 8, 0, 0}, // #90
  {8* 27, 8, 8, 8, 0, 0}, // #91
  {8* 28, 8, 8, 8, 0, 0}, // #92
  {8* 29, 8, 8, 8, 0, 0}, // #93
  {8* 30, 8, 8, 8, 0, 0}, // #94
  {8* 31, 8, 8, 8, 0, 0}, // #95
  {8* 64, 8, 8, 8, 0, 0}, // #96
  {8* 65, 8, 8, 8, 0, 0}, // #97
  {8* 66, 8, 8, 8, 0, 0}, // #98
  {8* 67, 8, 8, 8, 0, 0}, // #99
  {8* 68, 8, 8, 8, 0, 0}, // #100
  {8* 69, 8, 8, 8, 0, 0}, // #101
  {8* 70, 8, 8, 8, 0, 0}, // #102
  {8* 71, 8, 8, 8, 0, 0}, // #103
  {8* 72, 8, 8, 8, 0, 0}, // #104
  {8* 73, 8, 8, 8, 0, 0}, // #105
  {8* 74, 8, 8, 8, 0, 0}, // #106
  {8* 75, 8, 8, 8, 0, 0}, // #107
  {8* 76, 8, 8, 8, 0, 0}, // #108
  {8* 77, 8, 8, 8, 0, 0}, // #109
  {8* 78, 8, 8, 8, 0, 0}, // #110
  {8* 79, 8, 8, 8, 0, 0}, // #111
  {8* 80, 8, 8, 8, 0, 0}, // #112
  {8* 81, 8, 8, 8, 0, 0}, // #113
  {8* 82, 8, 8, 8, 0, 0}, // #114
  {8* 83, 8, 8, 8, 0, 0}, // #115
  {8* 84, 8, 8, 8, 0, 0}, // #116
  {8* 85, 8, 8, 8, 0, 0}, // #117
  {8* 86, 8, 8, 8, 0, 0}, // #118
  {8* 87, 8, 8, 8, 0, 0}, // #119
  {8* 88, 8, 8, 8, 0, 0}, // #120
  {8* 89, 8, 8, 8, 0, 0}, // #121
  {8* 90, 8, 8, 8, 0, 0}, // #122
  {8* 91, 8, 8, 8, 0, 0}, // #123
  {8* 92, 8, 8, 8, 0, 0}, // #124
  {8* 93, 8, 8, 8, 0, 0}, // #125
  {8* 94, 8, 8, 8, 0, 0}, // #126
  {8* 95, 8, 8, 8, 0, 0}, // #127
  {8*192, 8, 8, 8, 0, 0}, // #128
  {8*193, 8, 8, 8, 0, 0}, // #129
  {8*194, 8, 8, 8, 0, 0}, // #130
  {8*195, 8, 8, 8, 0, 0}, // #131
  {8*196, 8, 8, 8, 0, 0}, // #132
  {8*197, 8, 8, 8, 0, 0}, // #133
  {8*198, 8, 8, 8, 0, 0}, // #134
  {8*199, 8, 8, 8, 0, 0}, // #135
  {8*200, 8, 8, 8, 0, 0}, // #136
  {8*201, 8, 8, 8, 0, 0}, // #137
  {8*202, 8, 8, 8, 0, 0}, // #138
  {8*203, 8, 8, 8, 0, 0}, // #139
  {8*204, 8, 8, 8, 0, 0}, // #140
  {8*205, 8, 8, 8, 0, 0}, // #141
  {8*206, 8, 8, 8, 0, 0}, // #142
  {8*207, 8, 8, 8, 0, 0}, // #143
  {8*208, 8, 8, 8, 0, 0}, // #144
  {8*209, 8, 8, 8, 0, 0}, // #145
  {8*210, 8, 8, 8, 0, 0}, // #146
  {8*211, 8, 8, 8, 0, 0}, // #147
  {8*212, 8, 8, 8, 0, 0}, // #148
  {8*213, 8, 8, 8, 0, 0}, // #149
  {8*214, 8, 8, 8, 0, 0}, // #150
  {8*215, 8, 8, 8, 0, 0}, // #151
  {8*216, 8, 8, 8, 0, 0}, // #152
  {8*217, 8, 8, 8, 0, 0}, // #153
  {8*218, 8, 8, 8, 0, 0}, // #154
  {8*219, 8, 8, 8, 0, 0}, // #155
  {8*220, 8, 8, 8, 0, 0}, // #156
  {8*221, 8, 8, 8, 0, 0}, // #157
  {8*222, 8, 8, 8, 0, 0}, // #158
  {8*223, 8, 8, 8, 0, 0}, // #159
  {8* 96, 8, 8, 8, 0, 0}, // #160
  {8* 97, 8, 8, 8, 0, 0}, // #161
  {8* 98, 8, 8, 8, 0, 0}, // #162
  {8* 99, 8, 8, 8, 0, 0}, // #163
  {8*100, 8, 8, 8, 0, 0}, // #164
  {8*101, 8, 8, 8, 0, 0}, // #165
  {8*102, 8, 8, 8, 0, 0}, // #166
  {8*103, 8, 8, 8, 0, 0}, // #167
  {8*104, 8, 8, 8, 0, 0}, // #168
  {8*105, 8, 8, 8, 0, 0}, // #169
  {8*106, 8, 8, 8, 0, 0}, // #170
  {8*107, 8, 8, 8, 0, 0}, // #171
  {8*108, 8, 8, 8, 0, 0}, // #172
  {8*109, 8, 8, 8, 0, 0}, // #173
  {8*110, 8, 8, 8, 0, 0}, // #174
  {8*111, 8, 8, 8, 0, 0}, // #175
  {8*112, 8, 8, 8, 0, 0}, // #176
  {8*113, 8, 8, 8, 0, 0}, // #177
  {8*114, 8, 8, 8, 0, 0}, // #178
  {8*115, 8, 8, 8, 0, 0}, // #179
  {8*116, 8, 8, 8, 0, 0}, // #180
  {8*117, 8, 8, 8, 0, 0}, // #181
  {8*118, 8, 8, 8, 0, 0}, // #182
  {8*119, 8, 8, 8, 0, 0}, // #183
  {8*120, 8, 8, 8, 0, 0}, // #184
  {8*121, 8, 8, 8, 0, 0}, // #185
  {8*122, 8, 8, 8, 0, 0}, // #186
  {8*123, 8, 8, 8, 0, 0}, // #187
  {8*124, 8, 8, 8, 0, 0}, // #188
  {8*125, 8, 8, 8, 0, 0}, // #189
  {8*126, 8, 8, 8, 0, 0}, // #190
  {8*127, 8, 8, 8, 0, 0}, // #191
  {8* 64, 8, 8, 8, 0, 0}, // #192
  {8* 65, 8, 8, 8, 0, 0}, // #193
  {8* 66, 8, 8, 8, 0, 0}, // #194
  {8* 67, 8, 8, 8, 0, 0}, // #195
  {8* 68, 8, 8, 8, 0, 0}, // #196
  {8* 69, 8, 8, 8, 0, 0}, // #197
  {8* 70, 8, 8, 8, 0, 0}, // #198
  {8* 71, 8, 8, 8, 0, 0}, // #199
  {8* 72, 8, 8, 8, 0, 0}, // #200
  {8* 73, 8, 8, 8, 0, 0}, // #201
  {8* 74, 8, 8, 8, 0, 0}, // #202
  {8* 75, 8, 8, 8, 0, 0}, // #203
  {8* 76, 8, 8, 8, 0, 0}, // #204
  {8* 77, 8, 8, 8, 0, 0}, // #205
  {8* 78, 8, 8, 8, 0, 0}, // #206
  {8* 79, 8, 8, 8, 0, 0}, // #207
  {8* 80, 8, 8, 8, 0, 0}, // #208
  {8* 81, 8, 8, 8, 0, 0}, // #209
  {8* 82, 8, 8, 8, 0, 0}, // #210
  {8* 83, 8, 8, 8, 0, 0}, // #211
  {8* 84, 8, 8, 8, 0, 0}, // #212
  {8* 85, 8, 8, 8, 0, 0}, // #213
  {8* 86, 8, 8, 8, 0, 0}, // #214
  {8* 87, 8, 8, 8, 0, 0}, // #215
  {8* 88, 8, 8, 8, 0, 0}, // #216
  {8* 89, 8, 8, 8, 0, 0}, // #217
  {8* 90, 8, 8, 8, 0, 0}, // #218
  {8* 91, 8, 8, 8, 0, 0}, // #219
  {8* 92, 8, 8, 8, 0, 0}, // #220
  {8* 93, 8, 8, 8, 0, 0}, // #221
  {8* 94, 8, 8, 8, 0, 0}, // #222
  {8* 95, 8, 8, 8, 0, 0}, // #223
  {8* 96, 8, 8, 8, 0, 0}, // #224
  {8* 97, 8, 8, 8, 0, 0}, // #225
  {8* 98, 8, 8, 8, 0, 0}, // #226
  {8* 99, 8, 8, 8, 0, 0}, // #227
  {8*100, 8, 8, 8, 0, 0}, // #228
  {8*101, 8, 8, 8, 0, 0}, // #229
  {8*102, 8, 8, 8, 0, 0}, // #230
  {8*103, 8, 8, 8, 0, 0}, // #231
  {8*104, 8, 8, 8, 0, 0}, // #232
  {8*105, 8, 8, 8, 0, 0}, // #233
  {8*106, 8, 8, 8, 0, 0}, // #234
  {8*107, 8, 8, 8, 0, 0}, // #235
  {8*108, 8, 8, 8, 0, 0}, // #236
  {8*109, 8, 8, 8, 0, 0}, // #237
  {8*110, 8, 8, 8, 0, 0}, // #238
  {8*111, 8, 8, 8, 0, 0}, // #239
  {8*112, 8, 8, 8, 0, 0}, // #240
  {8*113, 8, 8, 8, 0, 0}, // #241
  {8*114, 8, 8, 8, 0, 0}, // #242
  {8*115, 8, 8, 8, 0, 0}, // #243
  {8*116, 8, 8, 8, 0, 0}, // #244
  {8*117, 8, 8, 8, 0, 0}, // #245
  {8*118, 8, 8, 8, 0, 0}, // #246
  {8*119, 8, 8, 8, 0, 0}, // #247
  {8*120, 8, 8, 8, 0, 0}, // #248
  {8*121, 8, 8, 8, 0, 0}, // #249
  {8*122, 8, 8, 8, 0, 0}, // #250
  {8*123, 8, 8, 8, 0, 0}, // #251
  {8*124, 8, 8, 8, 0, 0}, // #252
  {8*125, 8, 8, 8, 0, 0}, // #253
  {8*126, 8, 8, 8, 0, 0}, // #254
  {8* 94, 8, 8, 8, 0, 0}, // #255
};

const GFXfont cbmFont = { (uint8_t *) cbmFontData, (GFXglyph *) cbmFontGlyph, 0, 255, 8 };

#endif
