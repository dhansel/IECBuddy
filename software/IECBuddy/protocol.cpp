#include "protocol.h"

// these need to be defined in the implementation
bool send_data(uint32_t length, const uint8_t *buffer);
bool recv_data(uint32_t length, uint8_t *buffer);


const char *get_status_msg(StatusType status)
{
  switch( status )
    {
    case ST_OK:               return "OK";
    case ST_INVALID_COMMAND:  return "INVALID COMMAND";
    case ST_INVALID_DIR:      return "INVALID DIR";
    case ST_INVALID_FILE:     return "INVALID FILE";
    case ST_INVALID_LENGTH:   return "INVALID LENGTH";
    case ST_DRIVE_FULL:       return "DRIVE FULL";
    case ST_READ_ERROR:       return "READ ERROR";
    case ST_WRITE_ERROR:      return "WRITE ERROR";
    case ST_INVALID_DATA:     return "INVALID DATA";
    case ST_TIMEOUT:          return "TIMEOUT";
    case ST_CHECKSUM_ERROR:   return "CHECKSUM ERROR";
    case ST_FILE_EXISTS:      return "FILE EXISTS";
    case ST_FILE_NOT_FOUND:   return "FILE NOT FOUND";
    case ST_NOT_MOUNTED:      return "NO IMAGE MOUNTED";

    case ST_COM_ERROR:        return "COM ERROR";

    default:
      {
        static char msg[20];
#ifdef WIN32
        sprintf_s(msg, 20, "[ERROR %i]", status);
#else
        snprintf(msg, 20, "[ERROR %i]", status);
#endif
        return msg;
      }
    }
}

bool send_uint(uint32_t i)
{
  uint8_t data[4];
  data[0] = i & 255; i = i / 256;
  data[1] = i & 255; i = i / 256;
  data[2] = i & 255; i = i / 256;
  data[3] = i;
  return send_data(4, data);
}


bool recv_uint(uint32_t &i)
{
  uint8_t data[4];
  if( !recv_data(4, data) )
    return false;
  else
    {
      i = data[3];
      i = data[2] + i * 256;
      i = data[1] + i * 256;
      i = data[0] + i * 256;
      return true;
    }
}


bool send_sint(int32_t i)
{
  return send_uint((uint32_t) i);
}


bool recv_sint(int32_t &i)
{
  uint32_t u;
  if( recv_uint(u) ) 
    { i = u; return true; }
  else
    return false;
}


#define CMD_MAGIC 0xFEEDABCD

bool send_command(CommandType cmd)
{
  return send_uint(CMD_MAGIC) && send_uint(cmd);
}

CommandType recv_command()
{
  uint32_t d;
  CommandType cmd;

  while(1)
    {
      if( !recv_uint(d) )
        return CMD_INVALID;
      else if( d==MAGIC_PING_TO_DEVICE )
        {
          d = MAGIC_PING_FROM_DEVICE;
          send_uint(d);
        }
      else if( d==CMD_MAGIC && recv_uint(cmd) )
        return cmd;
      else
        return CMD_INVALID;
    }
}


bool send_status(StatusType status)
{
  return send_sint(status);
}


StatusType recv_status()
{
  StatusType status;
  if( !recv_sint(status) ) status = ST_COM_ERROR;
  return status;
}


bool send_string(const std::string &s)
{
  uint32_t len = (uint32_t) s.length();
  return send_uint(len) && send_data(len, (const uint8_t *) s.data());
}


bool recv_string(std::string &s)
{
  bool res = false;

  uint32_t len;
  if( recv_uint(len) )
    {
      char *data = (char *) malloc(len);
      if( data )
        {
          if( recv_data(len, (uint8_t *) data) )
            {
              s = std::string(data, len);
              res = true;
            }
          free(data);
        }
    }

  return res;
}
