#include "IECDisplay_ST7789.h"
#ifdef SUPPORT_ST7789

#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <algorithm>
#include <cctype>
#include "src/AnimatedGif/AnimatedGIF.h"
using namespace std;

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
  m_display->println(msg.c_str());
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
  m_display->setTextColor(color);
  m_display->setCursor(0, m_display->height()-m_display->getTextLineHeight()-10);
  m_display->clearCurrentLine();
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
  m_display->setTextColor(RGB565_WHITE);
  m_display->setCursor(0, 1*m_display->getTextLineHeight());
  if( clear ) m_display->clearCurrentLine();
  m_display->print(s.c_str());
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

  int w = 25;
  int x = m_display->width()-w;
  int y = 0;

  if( printing )
    {
      if( millis()>spinto )
        {
          m_display->clearRegion(x, y, w, m_display->getTextLineHeight());
          m_display->setTextSize(3);
          m_display->setTextColor(RGB565_YELLOW);
          m_display->setCursor(x, y);
          m_display->write(spinchr[spin]);
          spin   = (spin+1) & 3;
          spinto = millis()+250;
        }
    }
  else
    {
      m_display->clearRegion(x, y, w, m_display->getTextLineHeight());
      spinto = 0;
      spin   = 0;
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


#endif
