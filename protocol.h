#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstdint>

#define PRINTDATAFILE "$PRINTDATA$"

#define CMD_DIR              1
#define CMD_GETFILE          2
#define CMD_PUTFILE          3
#define CMD_DRIVESTATUS      4
#define CMD_DRIVECMD         5
#define CMD_MOUNT            6
#define CMD_UNMOUNT          7
#define CMD_GET_MOUNTED      8
#define CMD_SET_CONFIG_VAL   9
#define CMD_GET_CONFIG_VAL  10
#define CMD_CLEAR_CONFIG    11
#define CMD_DELETE_FILE     12
#define CMD_SHOW_BITMAP     13
#define CMD_SHOW_GIF        14
#define CMD_REBOOT          15
#define CMD_INVALID          0xFFFFFFFF

#define ST_OK                0
#define ST_INVALID_COMMAND   1
#define ST_INVALID_DIR       2
#define ST_INVALID_FILE      3
#define ST_INVALID_LENGTH    4 
#define ST_DRIVE_FULL        5 
#define ST_READ_ERROR        6
#define ST_WRITE_ERROR       7
#define ST_INVALID_DATA      8
#define ST_TIMEOUT           9
#define ST_CHECKSUM_ERROR   10
#define ST_FILE_EXISTS      11
#define ST_FILE_NOT_FOUND   12
#define ST_NOT_MOUNTED      13
#define ST_COM_ERROR        -1

#define FF_MODIFIED         0x00000001

#define MAGIC_PING_TO_DEVICE    0xFED55DEF
#define MAGIC_PING_FROM_DEVICE  0xFEDAADEF

typedef int32_t  StatusType;
typedef uint32_t CommandType;

bool send_uint(uint32_t i);
bool recv_uint(uint32_t &i);
bool send_sint(int32_t i);
bool recv_sint(int32_t &i);

bool        send_command(CommandType cmd);
CommandType recv_command();

bool       send_status(StatusType status);
StatusType recv_status();

bool send_string(const std::string &s);
bool recv_string(std::string &s);

const char *get_status_msg(StatusType status);

#endif
