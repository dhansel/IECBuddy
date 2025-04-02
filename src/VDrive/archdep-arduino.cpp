#if defined(ARDUINO)

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <malloc.h>

#include <Arduino.h>
#include <LittleFS.h>
#include "archdep.h"

extern "C"
{
#include "lib.h"
}

//#define DEBUG_ARCHDEP

#ifdef DEBUG_ARCHDEP
#define DBG(x) printf x;
#else
#define DBG(x)
#endif

#define F_WRITEMODE   0x01
#define F_INBUFFER    0x02
#define F_CANTBUFFER  0x04
#define F_CANTWRITE   0x08


static uint32_t getTotalHeap()
{
  extern char __StackLimit, __bss_end__;
  return &__StackLimit - &__bss_end__;
}


static uint32_t getUsedHeap()
{
  struct mallinfo m = mallinfo();
  return m.uordblks;
}

static uint32_t getAvailableHeap()
{
  return getTotalHeap() - getUsedHeap();
}


#define MAXFILES 3
static File     s_lfsFiles[MAXFILES];
static uint8_t  s_lfsFilesFlags[MAXFILES];
static uint8_t *s_imageData = NULL;
static uint32_t s_imageDataSize = 0, s_imageDataPos = 0;
static uint32_t s_imageDataMinWritePos = 0, s_imageDataMaxWritePos = 0;

void archdep_flush_memcache(ADFILE *f)
{
  int fidx = ((int) f)-1;

  if( fidx<0 || fidx>MAXFILES || !s_lfsFiles[fidx] )
    return;

  if( (s_imageData!=NULL) && (s_imageDataMinWritePos<s_imageDataMaxWritePos) )
    {
      uint32_t n = s_imageDataMaxWritePos-s_imageDataMinWritePos;
      DBG(("archdep_flush: writing memory data (offset %u, size %u) back to file '%s'...", 
           s_imageDataMinWritePos, n, s_lfsFiles[fidx].fullName()));
      char *name = strdup(s_lfsFiles[fidx].fullName());

      s_lfsFiles[fidx].close();

      bool ok = false;
      struct FSInfo info;
      if( LittleFS.info(info) && s_lfsFiles[fidx].size() < (info.totalBytes-info.usedBytes-6*info.blockSize) )
        {
          // we have enough space for LittleFS to write a new copy and then delete the old
          // so we can proceed with only writing the modified part
          s_lfsFiles[fidx] = LittleFS.open(name, "r+");
          s_lfsFiles[fidx].seek(s_imageDataMinWritePos, SeekSet);
          ok = s_lfsFiles[fidx] && s_lfsFiles[fidx].write(s_imageData+s_imageDataMinWritePos, n);
          if( ok ) s_lfsFiles[fidx].flush();
        }
      else
        {
          // not enough space on file system to do a partial rewrite (LittleFS first writes
          // the new file and then deletes the old) => rewrite the whole file
          s_lfsFiles[fidx] = LittleFS.open(name, "w");
          ok = s_lfsFiles[fidx].write(s_imageData, s_imageDataSize);
          s_lfsFiles[fidx].close();
          s_lfsFiles[fidx] = LittleFS.open(name, "r");
        }

      if( ok )
        { 
          s_imageDataMinWritePos = s_imageDataSize;
          s_imageDataMaxWritePos = 0;
          DBG(("DONE\n", 0)); 
        }
      else
        { DBG(("FAILED\n", 0)); }
      
      free(name);
    }
  else if( (s_lfsFilesFlags[fidx] & (F_CANTBUFFER|F_CANTWRITE))==F_CANTBUFFER )
    {
      DBG(("archdep_flush: flushing file '%s'...", s_lfsFiles[fidx].fullName()));
      s_lfsFiles[fidx].flush();
      DBG(("DONE\n", 0));
    }
}


static ADFILE *newFile(const char *filename, const char *mode)
{
  int fidx = 0;
  for(fidx=0; fidx<MAXFILES; fidx++)
    if( !s_lfsFiles[fidx] )
      break;

  if( fidx >= MAXFILES )
    return NULL; // cannot open another file
  else if( mode[0]=='r' )
    {
      s_lfsFiles[fidx] = LittleFS.open(filename, "r");
      s_lfsFilesFlags[fidx] = 0;
    }
  else if( mode[0]=='w' )
    {
      s_lfsFiles[fidx] = LittleFS.open(filename, "w");
      s_lfsFilesFlags[fidx] = F_CANTBUFFER|F_WRITEMODE;
    }

  return ((bool) s_lfsFiles[fidx]) ? (ADFILE *) (fidx+1) : NULL;
}


static void closeFile(ADFILE *f)
{
  int fidx = ((int) f)-1;
  if( fidx<0 || fidx>=MAXFILES || !s_lfsFiles[fidx] )
    return;

  if( s_imageData!=NULL )
    {
      archdep_flush_memcache(f);
      free(s_imageData);
      s_imageData = NULL;
    }
  
  s_lfsFiles[fidx].close();
  s_lfsFilesFlags[fidx] = 0;
}


static bool isFileInBuffer(ADFILE *f)
{
  int fidx = ((int) f)-1;
  if( fidx<0 || fidx>=MAXFILES )
    return false;
  else
    return s_lfsFiles[fidx] && (s_lfsFilesFlags[fidx]&F_INBUFFER)!=0 &&  s_imageData!=NULL;
}


uint8_t getFileFlags(ADFILE *f)
{
  int fidx = ((int) f)-1;
  if( fidx>=0 || fidx<MAXFILES )
    return s_lfsFilesFlags[fidx];
  else
    return 0;
}


File getFile(ADFILE *f, bool write = false)
{
  File file;
  int fidx = ((int) f)-1;
  if( fidx>=0 || fidx<MAXFILES )
    file = s_lfsFiles[fidx];

  if( file && write && (s_imageData==NULL) && (s_lfsFilesFlags[fidx] & F_CANTBUFFER)==0 )
    {
      File file = s_lfsFiles[fidx];
      size_t pos  = file.position();
      file.seek(0, SeekEnd);
      size_t size = file.size();
#if 0
      DBG(("\nHeap total : %u", getTotalHeap()));
      DBG(("\nHeap used  : %u", getUsedHeap()));
      DBG(("\nHeap avail : %u", getAvailableHeap()));
#endif
      
      s_imageData = (uint8_t *) malloc(size);
      if( s_imageData!=NULL )
        {
          DBG(("\ngetFile: loading file '%s' of size %u into memory...", file.fullName(), size));
          file.seek(0, SeekSet);
          if( file.read(s_imageData, size)==size )
            {
              DBG(("DONE", 0));
              s_imageDataPos = pos;
              s_imageDataSize = size;
              s_imageDataMinWritePos = size;
              s_imageDataMaxWritePos = 0;
              s_lfsFilesFlags[fidx] |= F_INBUFFER;
            }
          else
            {
              DBG(("FAILED", 0));
              file.seek(pos, SeekSet);
              free(s_imageData);
              s_imageData=NULL;
              s_lfsFilesFlags[fidx] |= F_CANTBUFFER;
            }
        }
      else
        {
          DBG(("\ngetFile: file '%s' of size %u can't fit into memory (heap available=%u)", file.fullName(), size, getAvailableHeap()));
          s_lfsFilesFlags[fidx] |= F_CANTBUFFER;
          file.seek(pos, SeekSet);
        }
    }

  if( file && write && (s_imageData==NULL) && (s_lfsFilesFlags[fidx] & (F_WRITEMODE|F_CANTWRITE))==0 )
    {
      size_t pos = file.position();
      char *name = strdup(file.fullName());

      struct FSInfo info;
#if 0
      LittleFS.info(info);
      DBG(( "\nblockSize=%lu pageSize=%lu totalBytes=%llu usedBytes=%llu freeBytes=%llu fileSize=%u",
            info.blockSize, info.pageSize, info.totalBytes, info.usedBytes, info.totalBytes-info.usedBytes, file.size()));
#endif
      // according to https://github.com/littlefs-project/littlefs/issues/533
      // LittleFS requires ~6 blocks of free storage space to work with
      if( LittleFS.info(info) && file.size() < (info.totalBytes-info.usedBytes-6*info.blockSize) )
        {
          DBG(("\ngetFile: reopen '%s' for writing at position %u\n", name, pos));
          // open again for writing now
          file.close();
          file = LittleFS.open(name, "r+");
          if( !file ) DBG(("  ERROR  ", 0));
          file.seek(pos, SeekSet);
          s_lfsFilesFlags[fidx] |= F_WRITEMODE;
        }
      else
        {
          DBG(("\ngetFile: cannot reopen file '%s' of size %u for writing (filesys available=%llu, need %u)",
               name, file.size(), info.totalBytes-info.usedBytes, file.size()+6*info.blockSize));
          s_lfsFilesFlags[fidx] |= F_CANTWRITE;
        }
          
      free(name);
    }
      
  return file;
}


int archdep_default_logger(const char *level_string, const char *txt)
{
  printf("VDrive: %s\n", txt);
  return 0;
}


int archdep_default_logger_is_terminal(void)
{
  return 1;
}


int archdep_expand_path(char **return_path, const char *orig_name)
{
  *return_path = lib_strdup(orig_name);
  return 0;
}


archdep_tm_t *archdep_get_time(archdep_tm_t *ats)
{
  ats->tm_wday = 0;
  ats->tm_year = 70;
  ats->tm_mon  = 0;
  ats->tm_mday = 1;
  ats->tm_hour = 0;
  ats->tm_min  = 0;
  ats->tm_sec  = 0;
  return ats;
}


int archdep_access(const char *pathname, int mode)
{
  int res = -1;

  if( mode==ARCHDEP_F_OK )
    res = archdep_file_exists(pathname) ? 0 : -1;
  else if( mode & ARCHDEP_X_OK )
    res = -1;
  else if( mode & ARCHDEP_W_OK )
    {
      struct FSInfo info;
      struct FSStat stat;

      // according to https://github.com/littlefs-project/littlefs/issues/533
      // LittleFS requires ~6 blocks of free storage space to work with
      if( LittleFS.stat(pathname, &stat) && LittleFS.info(info) && (stat.size < (info.totalBytes-info.usedBytes-6*info.blockSize) || stat.size < (getAvailableHeap()-1024)) )
        res = 0;
      else
        res = -1;
    }
  else if( mode & ARCHDEP_R_OK )
    {
      res = archdep_file_exists(pathname) ? 0 : -1;
    }

  DBG(("archdep_access: %s %i %i\n", pathname, mode, res));
  return res;
}


int archdep_stat(const char *filename, size_t *len, unsigned int *isdir)
{
  int res = -1;

  struct FSStat stat;
  if( LittleFS.stat(filename, &stat) )
    {
      if( len )   *len   = stat.size;
      if( isdir ) *isdir = stat.isDir;
      res = 0;
    }
  else
    { DBG(("LittleFS.stat failed for: %s\n", filename)); }

  DBG(("archdep_stat: %s %i %i %i\n", filename, res, len ? *len : -1, isdir ? *isdir : -1));
  return res;
}


bool archdep_file_exists(const char *path)
{
  bool res = LittleFS.exists(path);
  DBG(("archdep_file_exists: %s %i\n", path, res));
  return res;
}


char *archdep_tmpnam()
{
  char *res = lib_strdup("tmpfile");
  DBG(("archdep_tmpnam: %s\n", res));
  return res;
}


off_t archdep_file_size(ADFILE *stream)
{
  uint32_t s = getFile(stream).size();
  DBG(("archdep_file_size: %p %u\n", stream, s));
  return (off_t) s;
}


archdep_dir_t *archdep_opendir(const char *path, int mode)
{
  return NULL;
}


const char *archdep_readdir(archdep_dir_t *dir)
{
  return NULL;
}


void archdep_closedir(archdep_dir_t *dir)
{
}


int archdep_remove(const char *path)
{
  int res = LittleFS.remove(path) ? 0 : -1;
  DBG(("archdep_remove: %s %i\n", path, res));
  return res;
}


int archdep_rename(const char *oldpath, const char *newpath)
{
  return -1;
}


ADFILE *archdep_fnofile()
{
  return NULL;
}

ADFILE *archdep_fopen(const char* filename, const char* mode)
{
  ADFILE *res = NULL;
  DBG(("archdep_fopen: %s %s", filename, mode));
  res = newFile(filename, mode);
  DBG((" => %p\n", res));
  return res;
}


int archdep_fclose(ADFILE *file)
{
  DBG(("archdep_fclose: %p\n", file));
  closeFile(file);
  return 0;
}


size_t archdep_fread(void* buffer, size_t size, size_t count, ADFILE *f)
{
  DBG(("archdep_fread: %p %u %u ", f, size, count));

  size_t n;
  if( isFileInBuffer(f) )
    {
      n = min(size*count, s_imageDataSize-s_imageDataPos);
      memcpy(buffer, s_imageData+s_imageDataPos, n);
      s_imageDataPos += n;
    }
  else
    n = getFile(f).read((uint8_t *) buffer, size*count);

  DBG(("=> %u %u\n", n, n/size));
  if( n!=size*count ) { DBG(("READ FAILED!\n", 0)); }
  return n/size;
}


int archdep_fgetc(ADFILE *stream)
{
  if( isFileInBuffer(stream) )
    return s_imageDataPos<s_imageDataSize ? s_imageData[s_imageDataPos++] : -1;
  else
    return getFile(stream).read();
}


size_t archdep_fwrite(const void* buffer, size_t size, size_t count, ADFILE *stream)
{
  size_t n = 0;
  DBG(("archdep_fwrite: %p %u %u ", stream, size, count));

  File f = getFile(stream, true);
  if( isFileInBuffer(stream) )
    {
      n = min(size*count, s_imageDataSize-s_imageDataPos);
      memcpy(s_imageData+s_imageDataPos, buffer, n);
      s_imageDataMinWritePos = min(s_imageDataMinWritePos, s_imageDataPos);
      s_imageDataPos += n;
      s_imageDataMaxWritePos = max(s_imageDataMaxWritePos, s_imageDataPos);
    }
  else if( f && (getFileFlags(stream)&F_CANTWRITE)==0 )
    n = f.write((const uint8_t *) buffer, size*count);

  DBG(("=> %u %u\n", n, n/size));
  if( n!=size*count ) { DBG(("WRITE FAILED!\n", 0)); }
  return n/size;
}


long int archdep_ftell(ADFILE *stream)
{
  long int pos;

  if( isFileInBuffer(stream) )
    pos = s_imageDataPos;
  else 
    pos = (long int) getFile(stream).position();

  DBG(("archdep_ftell: %p %li\n", stream, pos));
  return pos;
}


int archdep_fseek(ADFILE *stream, long int offset, int whence)
{
  DBG(("archdep_fseek: %p %li %i ", stream, whence, offset));

  if( isFileInBuffer(stream) )
    {
      switch( whence )
        {
        case SEEK_SET: s_imageDataPos  = offset; break;
        case SEEK_CUR: s_imageDataPos += offset; break;
        case SEEK_END: s_imageDataPos  = s_imageDataSize-offset; break;
        }

      DBG(("=> %i %li\n", 0, s_imageDataPos));
      return 0;
    }
  else
    {
      bool ok = true;
      File f = getFile(stream);
      switch( whence )
        {
        case SEEK_SET: ok = f.seek(offset, SeekSet); break;
        case SEEK_CUR: ok = f.seek(offset, SeekCur); break;
        case SEEK_END: ok = f.seek(offset, SeekEnd); break;
        }

      DBG(("=> %i %li\n", 0, f.position()));
      if( !ok ) { DBG(("SEEK FAILED!\n", 0)); }
      return ok ? 0 : -1;
    }
}


int archdep_fflush(ADFILE *stream)
{
  DBG(("archdep_fflush: %p\n", stream));
  return 0;
}


void archdep_frewind(ADFILE *file)
{
  DBG(("archdep_frewind: %p\n", file));
  archdep_fseek(file, 0, SEEK_SET);
}


int archdep_fisopen(ADFILE *file)
{
  return file!=NULL;
}


int archdep_fissame(ADFILE *file1, ADFILE *file2)
{
  return (file1==file2);
}


int archdep_ferror(ADFILE *file)
{
  int res = (getFileFlags(file) & F_CANTWRITE) ? 1 : 0;
  DBG(("archdep_ferror: %p %i\n", file, res));
  return res;
}


void archdep_exit(int excode)
{
  DBG(("------------ EXIT -------------\n"));
  while(1);
}


void archdep_init()
{
}

#endif
