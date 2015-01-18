/*
 * Copyright (c) 2015 Kees Bakker.  All rights reserved.
 *
 * This file is part of UNBbee.
 */

#include <Arduino.h>
#include <Stream.h>
#include <avr/wdt.h>

#include "UNBbee.h"

#if ENABLE_UNBBEE_DIAG
#define diagPrint(...) { if (_diagStream) _diagStream->print(__VA_ARGS__); }
#define diagPrintLn(...) { if (_diagStream) _diagStream->println(__VA_ARGS__); }
#else
#define diagPrint(...)
#define diagPrintLn(...)
#endif


UNBbeeClass unbbee;


/*!
 * A wrapper for delay that also resets the WDT while waiting
 */
static inline void mydelay(unsigned long nrMillis)
{
  const unsigned long d = 10;
  while (nrMillis > d) {
    wdt_reset();
    delay(d);
    nrMillis -= d;
  }
  delay(nrMillis);
}

/*!
 * \brief Did we time out
 *
 * This is a small utility function to see if we timed out
 */
static inline bool isTimedOut(uint32_t ts)
{
  return (long)(millis() - ts) >= 0;
}

/*!
 * \brief Initialize the instance of UNBbeeClass
 *
 * This is basically a class constructor.
 */
void UNBbeeClass::init(Stream &stream)
{
  _myStream = &stream;
  _diagStream = 0;
  _echoOff = false;
}

void UNBbeeClass::switchEchoOff()
{
  if (!_echoOff) {
    // Suppress echoing
    if (!sendCommandWaitForOK_P(PSTR("ATE0"))) {
      return;
    }
    _echoOff = true;
  }
}

void UNBbeeClass::flushInput()
{
  int c;
  while ((c = _myStream->read()) >= 0) {
    diagPrint((char)c);
  }
}

/*
 * \brief Read a line of input from SIM900
 */
int UNBbeeClass::readLine(uint32_t ts_max)
{
  uint32_t ts_waitLF = 0;
  bool seenCR = false;
  int c;

  //diagPrintLn(F("readLine"));
  _UNBBEE_bufcnt = 0;
  while (!isTimedOut(ts_max)) {
    wdt_reset();
    if (seenCR) {
      c = _myStream->peek();
      // ts_waitLF is guaranteed to be non-zero
      if ((c == -1 && isTimedOut(ts_waitLF)) || (c != -1 && c != '\n')) {
        //diagPrint(F("readLine:  peek '")); diagPrint(c); diagPrintLn('\'');
        // Line ended with just <CR>. That's OK too.
        goto ok;
      }
      // Only \n should fall through
    }

    c = _myStream->read();
    if (c < 0) {
      continue;
    }
    diagPrint((char)c);                 // echo the char
    seenCR = c == '\r';
    if (c == '\r') {
      ts_waitLF = millis() + 50;        // Wait another .05 sec for an optional LF
    } else if (c == '\n') {
      goto ok;
    } else {
      // Any other character is stored in the line buffer
      if (_UNBBEE_bufcnt < UNBBEE_BUFLEN) {
        _UNBBEE_buffer[_UNBBEE_bufcnt++] = c;
      }
    }
  }

  diagPrintLn(F("readLine timed out"));
  return -1;            // This indicates: timed out

ok:
  _UNBBEE_buffer[_UNBBEE_bufcnt] = 0;     // Terminate with NUL byte
  //diagPrint(F(" ")); diagPrintLn(_SIM900_buffer);
  return _UNBBEE_bufcnt;

}

/*!
 * \brief Wait for a line with "OK"
 */
bool UNBbeeClass::waitForOK(uint16_t timeout)
{
  int len;
  uint32_t ts_max = millis() + timeout;
  while ((len = readLine(ts_max)) >= 0) {
    if (len == 0) {
      // Skip empty lines
      continue;
    }
    if (strcmp_P(_UNBBEE_buffer, PSTR("OK")) == 0) {
      return true;
    }
    if (strcmp_P(_UNBBEE_buffer, PSTR("ERROR")) == 0) {
      return false;
    }
    // Other input is skipped.
  }
  return false;         // This indicates: timed out
}

/*!
 * \brief Prepare for a new command
 */
void UNBbeeClass::sendCommandProlog()
{
  flushInput();
  mydelay(50);
  diagPrint(F(">> "));
}

/*
 * \brief Add a part of the command (don't yet send the final CR)
 */
void UNBbeeClass::sendCommandAdd(char c)
{
  diagPrint(c);
  _myStream->print(c);
}
void UNBbeeClass::sendCommandAdd(int i)
{
  diagPrint(i);
  _myStream->print(i);
}
void UNBbeeClass::sendCommandAdd(const char *cmd)
{
  diagPrint(cmd);
  _myStream->print(cmd);
}
void UNBbeeClass::sendCommandAdd_P(const char *cmd)
{
  diagPrint(reinterpret_cast<const __FlashStringHelper *>(cmd));
  _myStream->print(reinterpret_cast<const __FlashStringHelper *>(cmd));
}

/*
 * \brief Send the final CR of the command
 */
void UNBbeeClass::sendCommandEpilog()
{
  diagPrintLn();
  _myStream->print('\r');
}

void UNBbeeClass::sendCommand(const char *cmd)
{
  sendCommandProlog();
  sendCommandAdd(cmd);
  sendCommandEpilog();
}
void UNBbeeClass::sendCommand_P(const char *cmd)
{
  sendCommandProlog();
  sendCommandAdd_P(cmd);
  sendCommandEpilog();
}

/*
 * \brief Send a command to the SIM900 and wait for "OK"
 *
 * The command string should not include the <CR>
 * Return true, only if "OK" is seen. "ERROR" and timeout
 * result in false.
 */
bool UNBbeeClass::sendCommandWaitForOK(const char *cmd, uint16_t timeout)
{
  sendCommand(cmd);
  return waitForOK(timeout);
}
bool UNBbeeClass::sendCommandWaitForOK_P(const char *cmd, uint16_t timeout)
{
  sendCommand_P(cmd);
  return waitForOK(timeout);
}

/*
 * \brief Get a string value after sending a certain AT command
 *
 * \param cmd the AT command
 * \param reply the char buffer for the answer
 * \param size the size of the reply buffer
 * \param ts_max do not stay any longer than this timestamp
 * \returns true when there was no error, false otherwise
 *
 * Send the TD120x command and wait for the reply.
 * Finally the TD120x should give "OK"
 *
 * An example is:
 *   >> ATI7
 *   << 8622
 *   << OK
 */
bool UNBbeeClass::getStrValue(const char *cmd, char * reply, size_t size, uint32_t ts_max)
{
  sendCommand(cmd);

  int len;
  while ((len = readLine(ts_max)) >= 0) {
    if (len == 0) {
      // Skip empty lines
      continue;
    }
    strncpy(reply, _UNBBEE_buffer, size - 1);
    reply[size - 1] = '\0';             // terminate the string just in case strncpy didn't do this
    break;
  }
  if (len < 0) {
      // There was a timeout
      return false;
  }
  // Wait for "OK"
  return waitForOK();
}

/*!
 * Send message (AT$SS)
 */
bool UNBbeeClass::sendMessage(const char * msg)
{
  switchEchoOff();
  sendCommandProlog();
  sendCommandAdd_P(PSTR("AT$SS="));
  sendCommandAdd(msg);
  sendCommandEpilog();
  return waitForOK(20000);
}

/*!
 * \brief Get the device ID
 *
 * Send a "ATI7" command to the device and read the reponse.
 * Also wait for the "OK" that comes after it
 */
bool UNBbeeClass::getDeviceID(char *buffer, size_t buflen)
{
  switchEchoOff();
  uint32_t ts_max = millis() + 2000;
  return getStrValue("ATI7", buffer, buflen, ts_max);
}
