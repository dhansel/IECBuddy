#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string.h>

#include "../IECBuddy/protocol.h"
#include "commfun.h"
#include "utilfun.h"

#ifdef WIN32
#include <Windows.h>
#endif


void printUsage()
{
  printf("Usage: SKTool [-p COMx] dir|ls|rm|del|delete|cp|copy\n\n"
         "       SKTool ls|dir [patterns]            Display directory of files on IECBuddy\n"
         "       SKTool rm|del patterns              Delete files matching pattern on IECBuddy\n"
         "       SKTool cp|copy [-f] patterns SK:    Copy local files matching pattern to IECBuddy\n"
         "       SKTool cp|copy [-f] patterns dir    Copy IECBuddy files matching pattern to local directory dir\n"
         "       SKTool cp|copy [-f] name1 SK:name2  Copy local file name1 to IECBuddy as name2\n"
         "       SKTool cp|copy [-f] name1 name2     Copy IECBuddy file file name1 to local file name2\n"
         "       SKTool boot                         Switch IECBuddy to boot mode for firmware upload\n\n"
         "       If -p COMx is given then use port COMx, otherwise auto-detect\n"
         "       If -f is given for cp/copy then overwrite destination file if it exists\n"
         "       File name patterns can include '*' and '?' with their usual meaning.\n");
}


void getLocalFiles(char* pattern, vector<string> &files)
{
#ifdef WIN32
  WIN32_FIND_DATAA findFileData;
  HANDLE hFind = FindFirstFileA(pattern, &findFileData);

  if( hFind != INVALID_HANDLE_VALUE ) 
    {
      string dir;
      char *sep = strrchr(pattern, '\\');
      if( sep==NULL ) sep = strrchr(pattern, '/');
      if( sep!=NULL ) dir = string(pattern, sep-pattern+1);

      do 
        {
          // Exclude directories and "." or ".." entries
          if( !(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            files.push_back(dir + findFileData.cFileName);
        }
      while (FindNextFileA(hFind, &findFileData) != 0);
      
      FindClose(hFind); // Close the search handle
    }
#else
  // Linux/Unix automatically expamd patterns in command line arguments
  if ( exists(pattern) && !isDir(pattern) )
    files.push_back(pattern);
  else
    printf("Source file %s not not found.\n", pattern);
#endif
}


vector<string> getPatterns(int argc, char** argv)
{
  vector<string> patterns;

  for (int i = 0; i < argc; i++)
    {
      if (argv[i][0] == ':')
        patterns.push_back(argv[i] + 1);
      else if (strncmp(argv[i], "SK:", 3) == 0)
        patterns.push_back(argv[i] + 3);
      else
        patterns.push_back(argv[i]);
    }

  return patterns;
}


void cmdDir(int argc, char** argv)
{
  vector<string> patterns;
  if (argc > 0) patterns = getPatterns(argc, argv);
  printDir(patterns);
}


void cmdDelete(int argc, char** argv)
{
  vector<string> patterns = getPatterns(argc, argv);
  vector<string> filenames;
  StatusType status = getDirFiles(filenames, patterns);

  if (status == ST_OK && !patterns.empty())
    {
      for (vector<string>::iterator it = filenames.begin(); status == ST_OK && it != filenames.end(); ++it)
        {
          printf("Deleting file: %s\n", (*it).c_str());
          status = deleteFile(*it);
        }
    }

  if (status != ST_OK)
    printf("Error: %s (%i)\n", get_status_msg(status), status);
}


void cmdCopyFromSK(const vector<string>& patterns, const char* dest, bool overwrite)
{
  vector<string> filenames;
  StatusType status = getDirFiles(filenames, patterns);
  if (status == ST_OK)
    {
      if (filenames.size() > 1 && !isDir(dest))
        printf("Destination must be a directory if more than one file is copied!\n");
      else
        {
          for (vector<string>::iterator it = filenames.begin(); status == ST_OK && it != filenames.end(); ++it)
            {
              string name = *it;
              string dosname;
              if (filenames.size() == 1 && !isDir(dest))
                dosname = dest;
              else
                dosname = string(dest) + "\\" + fromPETSCII(name);

              if (exists(dosname.c_str()) && !overwrite)
                {
                  printf("Destination file '%s' exists, use '-f' to force overwrite.\n", dosname.c_str());
                }
              else
                {
                  printf("Copying SK:%s to %s\n", name.c_str(), dosname.c_str());
                  status = getFile(name, dosname);
                }
            }
        }
    }

  if (status != ST_OK)
    printf("Error: %s (%i)\n", get_status_msg(status), status);
}


void cmdCopyToSK(const vector<string>& filenames, const char* dest, bool overwrite)
{
  StatusType status = ST_OK;
  if (filenames.size() > 1 && *dest != 0)
    printf("Destination must be just SK: if more than one file is copied!\n");
  else
    {
      for (vector<string>::const_iterator it = filenames.begin(); status == ST_OK && it != filenames.end(); ++it)
        {
          string name = *it;
          size_t sep = name.find_last_of('\\');
          if (sep == string::npos) sep = name.find_last_of('/');

          string petname;
          if (*dest == 0)
            petname = (sep == string::npos) ? name : name.substr(sep + 1);
          else
            petname = string(dest);

          petname = toPETSCII(petname);
          printf("Copying %s to SK:%s\n", name.c_str(), petname.c_str());
          status = putFile(name, petname);
          if (status == ST_INVALID_FILE)
            {
              printf("Error: destination file name must include a 3-letter extension.\n");
              status = ST_OK;
            }
          else if (status == ST_FILE_EXISTS)
            {
              if (overwrite)
                {
                  status = deleteFile(petname);
                  if (status == ST_OK)
                    status = putFile(name, petname);
                }
              else
                {
                  printf("Destination file '%s' exists, use '-f' to force overwrite.\n", petname.c_str());
                  status = ST_OK;
                }
            }
        }
    }

  if (status != ST_OK)
    printf("Error: %s (%i)\n", get_status_msg(status), status);
}


void cmdCopy(int argc, char** argv)
{
  bool overwrite = false;

  int firstarg = 0;
  if (strcmp(argv[firstarg], "-f") == 0)
    {
      overwrite = true;
      firstarg++;
    }

  const char* dest = argv[argc - 1];
  if (dest[0] == ':' || strncmp(dest, "SK:", 3) == 0)
    {
      dest += dest[0] == ':' ? 1 : 3;
      vector<string> files;
      for(int i = firstarg; i < argc - 1; i++)
        getLocalFiles(argv[i], files);
      cmdCopyToSK(files, dest, overwrite);
    }
  else
    {
      vector<string> patterns = getPatterns(argc - 1, argv);
      cmdCopyFromSK(patterns, dest, overwrite);
    }
}


int main(int argc, char** argv)
{
  int firstarg = 1;
  const char* comPort = NULL;

  if( argc > firstarg + 1 && strcmp(argv[firstarg], "-p") == 0 )
    {
      comPort = argv[2];
      firstarg += 2;
    }

  if(argc <= firstarg)
    {
      printUsage();
    }
  else if( strcmp(argv[firstarg], "boot")==0 )
    {
      string port;
      if( comPort==NULL ) 
        {
          port = autodetectPort();
          if( port.empty() ) 
            printf("No port specified and autodetect failed.\n");
          else
            printf("Found IECBuddy on port %s\n", port.c_str());
        }
      else
        port = comPort;

      if( !port.empty() )
        {
          printf("Switching IECBuddy on port '%s' to boot mode.\n", port.c_str());
          comResetToBoot(port);
        }
    }
  else if (comOpen(comPort) == 0)
    {
      string cmd = argv[firstarg++];

      if (cmd == "ls" || cmd == "dir")
        cmdDir(argc - firstarg, argv + firstarg);
      else if (argc > firstarg && (cmd == "rm" || cmd == "del" || cmd == "delete"))
        cmdDelete(argc - firstarg, argv + firstarg);
      else if (argc > firstarg + 1 && (cmd == "cp" || cmd == "copy"))
        cmdCopy(argc - firstarg, argv + firstarg);
      else
        printUsage();

      comClose();
    }
  else if (comPort == NULL)
    printf("Unable to find COM port for IECBuddy.\n");
  else
    printf("Error opening port: %s\n", comPort);

  return 0;
}
