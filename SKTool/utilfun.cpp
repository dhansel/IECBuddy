#include "utilfun.h"
#include <sys/stat.h>

#define SHOW_LOWERCASE 0


uint8_t toPETSCII(uint8_t c)
{
  if( c>=65 && c<=90 && SHOW_LOWERCASE )
    c += 32;
  else if( c>=97 && c<=122 )
    c -= 32;
  
  return c;
}


string toPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    rs += char(toPETSCII(s[i]));
  
  return rs;
}


uint8_t fromPETSCII(uint8_t c)
{
  if( c==0xFF )
    c = '~';
  else if( c>=192 ) 
    c -= 96;
  
  if( c>=65 && c<=90 )
    c += 32;
  else if( c>=97 && c<=122 )
    c -= 32;
  
  return c;
}


string fromPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    rs += fromPETSCII(s[i]);

  return rs;
}


int exists(const char *name)
{
  struct stat statrec;
  return stat(name, &statrec)==0;
}


int isDir(const char *name)
{
  int res = 0;
  struct stat statrec;

  if( stat(name, &statrec)==0 ) 
    res = (statrec.st_mode & S_IFDIR)!=0;

  return res;
}


int isMatch(const char *pattern, const char *filename)
{
  const char *p = pattern, *f = filename;
  const char *star = NULL, *star_match = NULL;
  while (*f) 
    {
      if (*p == '*') 
        {
          // Collapse multiple '*'s
          while (*p == '*') p++;
          star = p;
          star_match = f;
          if( *p == 0 ) return 1;
        }

      if( *p == '?' || *p == *f )
        {
          p++;
          f++;
        }
      else if( star ) 
        {
          // '*' previously matched, so backtrack
          p = star;
          f = ++star_match;
        }
      else
        return 0;
    }

  // Skip trailing '*'s
  while (*p == '*') p++;
  return *p == 0;
}


int isMatch(const vector<string> &patterns, const char *filename)
{
  for(vector<string>::const_iterator it = patterns.begin(); it != patterns.end(); ++it)
    if( (*it).empty() || isMatch((*it).c_str(), filename) )
      return 1;

  return 0;
}
