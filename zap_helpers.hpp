#pragma once

#include <stdint.h>

namespace zap {

extern const int INVALID_HEXIT;

// IndifferentString represents a constant string that could be resident in
// either ROM or RAM. Ownership concerns are out of scope.
//
// TODO: work out how to make this a zero-cost abstraction on platforms that
// do not distinguish between ROM/RAM.
class IndifferentString {
 public:
  IndifferentString() : str_((const char *)nullptr), ram_(true) {}
  IndifferentString(const char *str) : str_(str), ram_(true) {}
  IndifferentString(const __FlashStringHelper *str) : str_(str), ram_(false) {}

  IndifferentString(const IndifferentString &other)
      : str_(other.str_), ram_(other.ram_) {}

  bool isRAM() const { return ram_; }
  bool isROM() const { return !ram_; }

  operator const char *() const { return str_.ram; }
  operator const __FlashStringHelper *() const { return str_.rom; }

 private:
  union str {
    str(const char *v) : ram(v) {}
    str(const __FlashStringHelper *v) : rom(v) {}
    const char *ram;
    const __FlashStringHelper *rom;
  } str_;
  bool ram_;
};

// Convert v (0 <= v <= 15) to it's ASCII hex equivalent
char toHex(uint8_t v);

// Returns true if ch is a-z or A-Z
bool isAlpha(char ch);

// Returns true if ch is 0-9
bool isNumeric(char ch);

// Returns true if ch is 0-9, a-z, or A-Z
bool isHexit(char ch);

// Decode an ASCII hex character to its integer value.
// Returns 0xFF (INVALID_HEXIT) if the value is invalid.
uint8_t decodeHexit(char ch);

// Returns true if ch is a valid Zap start of word character
bool isWordStartChar(char ch);

// Returns true if ch is a valid Zap word character
bool isWordChar(char ch);

// Compares a string to an entry in the string table, returning
// true if the two are equal.
bool streq(int strTableIx, const char *str);

// Return a PROGMEM pointer to an item in the string table
const char *strptr(int strTableIx);

};  // namespace zap
