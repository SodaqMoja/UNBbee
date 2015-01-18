#ifndef UNBBEE_H_
#define UNBBEE_H_
/*
 * Copyright (c) 2015 Kees Bakker.  All rights reserved.
 *
 * This file is part of UNBbee.
 */

#include <stdint.h>
#include <Stream.h>


// Comment this line, or make it an undef to disable
// diagnostic
#define ENABLE_UNBBEE_DIAG      1


class UNBbeeClass
{
public:
  void init(Stream &stream);
  void setDiag(Stream &stream) { _diagStream = &stream; }
  void setDiag(Stream *stream) { _diagStream = stream; }

  bool getDeviceID(char *buffer, size_t buflen);

private:

  void switchEchoOff();

  void flushInput();

  int readLine(uint32_t ts_max);

  bool waitForOK(uint16_t timeout=4000);

  void sendCommandProlog();
  void sendCommandAdd(char c);
  void sendCommandAdd(int i);
  void sendCommandAdd(const char *cmd);
  void sendCommandAdd_P(const char *cmd);
  void sendCommandEpilog();

  void sendCommand(const char *cmd);
  void sendCommand_P(const char *cmd);

  bool sendCommandWaitForOK(const char *cmd, uint16_t timeout=4000);
  bool sendCommandWaitForOK_P(const char *cmd, uint16_t timeout=4000);

  bool getIntValue(const char *cmd, const char *reply, int * value, uint32_t ts_max);
  bool getStrValue(const char *cmd, char * reply, size_t size, uint32_t ts_max);

#define UNBBEE_BUFLEN 64
  char _UNBBEE_buffer[UNBBEE_BUFLEN + 1];       // +1 for the 0 byte
  int _UNBBEE_bufcnt;

  Stream *_myStream;
  Stream *_diagStream;
  bool _echoOff;
};

extern UNBbeeClass unbbee;

#endif /* UNBBEE_H_ */
