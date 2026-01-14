#ifndef UTILFUN_H
#define UTILFUN_H

#include <string>
#include <vector>
using namespace std;

uint8_t toPETSCII(uint8_t c);
string toPETSCII(const string &s);
uint8_t fromPETSCII(uint8_t c);
string fromPETSCII(const string &s);

int exists(const char *name);
int isDir(const char *name);
int isMatch(const char *pattern, const char *filename);
int isMatch(const vector<string> &patterns, const char *filename);

#endif
