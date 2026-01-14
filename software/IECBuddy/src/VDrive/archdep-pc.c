#if !defined(ARDUINO) && defined(__GNUC__)

#ifdef WIN32
#include <windows.h>
#else
#define _ftelli64 ftell
#define _fseeki64 fseek
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#include "archdep.h"
#include "lib.h"


//#define DEBUG_ARCHDEP

#ifdef DEBUG_ARCHDEP
#define DBG(x) printf  x
#else
#define DBG(x)
#endif


uint32_t archdep_get_available_heap()
{
  return 0;
}


void archdep_flush_memcache(ADFILE *f)
{
}


int archdep_default_logger(const char *level_string, const char *txt)
{
  printf("%s\n", txt);
  fflush(stdout);
  return 0;
}


int archdep_default_logger_is_terminal(void)
{
  return 1;
}


archdep_tm_t *archdep_get_time(archdep_tm_t *ats)
{
  time_t timep;
  struct tm *ts;

  time(&timep);
  ts = localtime(&timep);
 
  ats->tm_wday = ts->tm_wday;
  ats->tm_year = ts->tm_year;
  ats->tm_mon  = ts->tm_mon;
  ats->tm_mday = ts->tm_mday;
  ats->tm_hour = ts->tm_hour;
  ats->tm_min  = ts->tm_min;
  ats->tm_sec  = ts->tm_sec;

  return ats;
}


int archdep_expand_path(char **return_path, const char *orig_name)
{
  *return_path = lib_strdup(orig_name);
  return 0;
}


int archdep_access(const char *pathname, int mode)
{
  int res = 0;
  if( mode==ARCHDEP_ACCESS_F_OK )
    res = access(pathname, F_OK);
  else 
    res = access(pathname, ((mode & ARCHDEP_ACCESS_R_OK) ? R_OK : 0) | ((mode & ARCHDEP_ACCESS_W_OK) ? W_OK : 0) | ((mode & ARCHDEP_ACCESS_X_OK) ? X_OK : 0));

  DBG(("archdep_access: %s %i %i\n", pathname, mode, res));
  return res;
}


int archdep_stat(const char *filename, size_t *len, unsigned int *isdir)
{
  struct stat statrec;

  int e = stat(filename, &statrec);
  if( e==0 )
    {
      if( len!=NULL )   *len = statrec.st_size;
      if( isdir!=NULL ) *isdir = (statrec.st_mode & S_IFDIR)!=0;
    }

  DBG(("archdep_stat: %s %u %i\n", filename, (uint32_t) (len ? *len : -1), isdir ? *isdir : -1));
  return e;
}


bool archdep_file_exists(const char *path)
{
#ifdef WIN32
  return (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES);
#else
  return access(path, F_OK) == 0;
#endif
}


char *archdep_tmpnam()
{
#ifdef WIN32
  char *temp_path;
  char *temp_name;

  temp_path = lib_malloc(MAX_PATH  + 1U);
  temp_name = lib_malloc(MAX_PATH  + 1U);

  if (GetTempPath(MAX_PATH , temp_path) == 0) {
    log_error(LOG_DEFAULT, "failed to get Windows temp dir.");
    lib_free(temp_path);
    lib_free(temp_name);
    exit(1);
  }

  if (GetTempFileName(temp_path, "vice", 0, temp_name) == 0) {
    log_error(LOG_DEFAULT, "failed to construct a Windows temp file.");
    lib_free(temp_path);
    lib_free(temp_name);
    exit(1);
  }

  lib_free(temp_path);
  DBG(("archdep_tmpnam: %s\n", temp_name));
  return temp_name;
#else
  char *temp_name = malloc(L_tmpnam);
  tmpnam(temp_name);
  return temp_name;
#endif
}


off_t archdep_file_size(ADFILE *stream)
{
  off_t pos, end;

  pos = ftell(stream);
  fseek(stream, 0, SEEK_END);
  end = ftell(stream);
  fseek(stream, pos, SEEK_SET);

  DBG(("archdep_file_size: %p %u\n", stream, (uint32_t) end));
  return end;
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
  int res = remove(path);
  DBG(("archdep_remove: %s %i\n", path, res));
  return res;
}


int archdep_rename(const char *oldpath, const char *newpath)
{
  return 0;
}


ADFILE *archdep_fnofile()
{
  return NULL;
}


ADFILE *archdep_fopen(const char* filename, const char* mode)
{
  DBG(("archdep_fopen: %s %s\n", filename, mode));
  ADFILE *res = fopen(filename, mode);
  return res;
}


int archdep_fclose(ADFILE *file)
{
  DBG(("archdep_fclose: %p\n", file));
  return fclose(file);
}


size_t archdep_fread(void* buffer, size_t size, size_t count, ADFILE *stream)
{
  DBG(("archdep_fread: %p %u %u ", stream, (uint32_t) size, (uint32_t) count));
  size_t n = fread(buffer, size, count, stream);
  DBG(("=> %i %u %u\n", ferror(stream), (uint32_t) n, (uint32_t) ftell(stream)));
  return n;
}


int archdep_fgetc(ADFILE *stream)
{
  return fgetc(stream);
}


size_t archdep_fwrite(const void* buffer, size_t size, size_t count, ADFILE *stream)
{
  DBG(("archdep_fwrite: %p %u %u ", stream, (uint32_t) size, (uint32_t) count));
  size_t n = fwrite(buffer, size, count, stream);
  DBG(("=> %i %u\n", ferror(stream), (uint32_t) n));
  return n;
}


long int archdep_ftell(ADFILE *stream)
{
  DBG(("archdep_ftell: %p ", stream));
  long int res = (long int) ftell(stream);
  DBG(("=> %li\n", res));
  return res;
}


int archdep_fseek(ADFILE *stream, long int offset, int whence)
{
  DBG(("archdep_fseek: %p %i %li ", stream, whence, offset));
  int res = fseek(stream, offset, whence);
  DBG(("=> %i %lu\n", res, ftell(stream)));
  return res;
}


int archdep_fflush(ADFILE *file)
{
  return fflush(file);
}


void archdep_frewind(ADFILE *file)
{
  DBG(("archdep_rewind: %p\n", file));
  return rewind(file);
}


int archdep_fisopen(ADFILE *file)
{
  return file!=NULL;
}


int archdep_fissame(ADFILE *file1, ADFILE *file2)
{
  return file1 == file2;
}


int archdep_ferror(ADFILE *file)
{
  return ferror(file);
}


void archdep_exit(int excode)
{
  exit(0);
}

#endif
