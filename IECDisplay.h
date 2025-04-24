#ifndef IECDISPLAY
#define IECDISPLAY

#include <string>

class IECDisplay
{
 public:
  IECDisplay();
  virtual ~IECDisplay();

  void setCurrentImageName(std::string iname);
  void setCurrentFileName(std::string fname);

  virtual void begin();

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);

  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void update(const char *statusMessage);

 protected:
  bool isErrorStatus(const char *statusMessage);

  std::string m_curImageName;
  std::string m_curFileName;

  int         m_curFileSize;
  int         m_curFileBytesRead;
  int         m_progressWidth;
};

#endif
