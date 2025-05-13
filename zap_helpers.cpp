#include "Zap.hpp"

namespace zap {

const int INVALID_HEXIT = 0xFF;

char toHex(uint8_t v) {
  if (v <= 9) return '0' + v;
  return 'A' + v - 10;
}

bool isAlpha(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }

bool isNumeric(char ch) { return ch >= '0' && ch <= '9'; }

bool isHexit(char ch) {
  return isNumeric(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

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

bool isWordStartChar(char ch) { return isAlpha(ch) || ch == '_'; }

bool isWordChar(char ch) {
  return isAlpha(ch) || isNumeric(ch) || ch == '_' || ch == '.' || ch == '/' ||
         ch == '?' || ch == '!' || ch == '-';
}

bool streq(int strTableIx, const char* str) {
  return strcmp_P(str, strptr(strTableIx)) == 0;
}

const char* strptr(int strTableIx) {
  return (const char*)pgm_read_word(&(string_table[strTableIx]));
}

};  // namespace zap