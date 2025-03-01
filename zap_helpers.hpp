#pragma once

#include <avr/pgmspace.h>

namespace zap {

bool isAlpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool isNumeric(char ch) {
  return ch >= '0' && ch <= '9';
}

bool isHexit(char ch) {
  return isNumeric(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

const int INVALID_HEXIT = 0xFF;

uint8_t decodeHexit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else {
    return INVALID_HEXIT;
  }
}

bool isWordStartChar(char ch) {
  return isAlpha(ch) || ch == '_' || ch == '!';
}

bool isWordTailChar(char ch) {
  return isAlpha(ch) || isNumeric(ch) || ch == '_' || ch == '.' || ch == '/' || ch == '?' || ch == '!';
}

class IndifferentString {
public:
  IndifferentString() : str_((const char*)nullptr), ram_(true) {}
  IndifferentString(const char *str) : str_(str), ram_(true) {}
  IndifferentString(__FlashStringHelper *str) : str_(str), ram_(false) {}
  IndifferentString(const IndifferentString &other) : str_(other.str_), ram_(other.ram_) {}

  bool isRAM() const { return ram_; }
  bool isROM() const { return !ram_; }

  operator const char*() const { return str_.ram; }
  operator const __FlashStringHelper*() const { return str_.rom; }

private:
  union str {
    str(const char *v) : ram(v) {}
    str(__FlashStringHelper *v) : rom(v) {}
    const char *ram;
    __FlashStringHelper *rom;
  } str_;
  bool ram_;
};

};