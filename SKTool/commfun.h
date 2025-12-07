#ifndef COMMFUN_H
#define COMMFUN_H

#include <vector>
#include <string>
using namespace std;

typedef int32_t StatusType;

int  comOpen(const char *port);
void comClose();

StatusType printDir(const vector<string> &patterns);
StatusType printDir(const char *pattern = NULL);

StatusType getDirFiles(vector<string> &filenames, const vector<string> &patterns);
StatusType getDirFiles(vector<string> &filenames, const char *pattern = NULL);

StatusType getDriveStatus(string &drivestatus);
StatusType sendDriveCommand(const string &cmd);

StatusType putFile(const string &fname, string dstfname = "");
StatusType getFile(const string &fname, string dstfname = "");
StatusType deleteFile(const string &fname);

StatusType mountDisk(const string &name);
StatusType unmountDisk();
StatusType getMountedDisk();

StatusType setConfigValue(const string &key, const string &value);
StatusType getConfigValue(const string &key);
StatusType clearConfig();

StatusType sendBitmap(const string &fname, int32_t x, int32_t y, uint32_t w, uint32_t h);
StatusType sendGIF(const string &fname, int32_t x, int32_t y);

StatusType reboot();

#endif
