#pragma once

#include "zap_helpers.hpp"

namespace zap {

#define TOK_ERROR         -1
#define TOK_WORD          1
#define TOK_STRING        2
#define TOK_KEY           3
#define TOK_INT           4
#define TOK_HEX           5
#define TOK_FLOAT         6
#define TOK_BOOL          7

struct Arg {
  int index;
  const char *key;
  union {
    bool B;
    const char *S;
    int I;
    float F;
  };

  inline bool positional() { return key == nullptr; }
  inline bool named() { return key != nullptr; }
};

class ArgParser {
public:
  ArgParser(char *args, int len)
    : args_(args), len_(len), rp_(0)
  {
    skipSpace();
  }

  bool remain() const { return rp_ < len_; }
  bool end() const { return !remain(); }

  bool scanWord(Arg *dst) {
    char tok = lex(dst);
    return tok == TOK_WORD;
  }

  bool scanString(Arg *dst) {
    char tok = lex(dst);
    return tok == TOK_WORD || tok == TOK_STRING;
  }

  bool scanInt(Arg *dst) {
    char tok = lex(dst);
    return tok == TOK_INT || tok == TOK_HEX;
  }

  bool scanBool(Arg *dst) {
    char tok = lex(dst);
    return tok == TOK_BOOL;
  }

private:
  char lex(Arg *dst) {
    char ch = curr();
    if (isAlpha(ch)) {
      return parseWBK(dst);
    } else if (isNumeric(ch)) {
      return parseNumber(dst, false);
    } else if (ch == '-') {
      adv();
      return parseNumber(dst, true);
    } else {
      return TOK_ERROR;
    }
  }

  bool strCmp(const char *inputText, const char *cmpText, int len) {
    for (int i = 0; i < len; i++) {
      if (inputText[i] != cmpText[i]) {
        return false;
      }
    }
    return true;
  }

  char parseWBK(Arg *dst) {
    int len;
    char *text = lexWord(&len);
    if (text == nullptr) {
      return TOK_ERROR;
    }

    char tok;

    if ((len == 2 && strCmp(text, "on", 2))
        || (len == 3 && strCmp(text, "yes", 3))
        || (len == 4 && strCmp(text, "true", 4))) {
      dst->B = true;
      tok = TOK_BOOL;
    } else if ((len == 3 && strCmp(text, "off", 3))
                || (len == 2 && strCmp(text, "no", 2))
                || (len == 5 && strCmp(text, "false", 5))) {
      dst->B = false;
      tok = TOK_BOOL;
    } else if (curr() == ':') {
      args_[rp_++] = 0;
      dst->key = text;
      tok = TOK_KEY;
    } else {
      args_[rp_++] = 0;
      dst->S = text;
      tok = TOK_WORD;
    }

    skipSpace();
    
    return tok;
  }

  // Read the next number.
  // Returns the token type that was read, or TOK_ERROR on error.
  // The decoded numeric value is stored in the appropriate field of dst.
  char parseNumber(Arg *dst, bool negate) {
    if (curr() == '0' && peek() == 'x') {
      adv();
      adv();
      int i = lexHexInt();
      if (i < 0) {
        return TOK_ERROR;
      }
      dst->I = negate ? -i : i;
      skipSpace();
      return TOK_INT;
    }

    int ipart = lexDecInt();
    if (ipart < 0) {
      return TOK_ERROR;
    }

    if (curr() == '.') {
      adv();
      int fpart = lexDecInt();
      if (fpart < 0) {
        return TOK_ERROR;
      }
      // TODO: !!!
      dst->F = 0.0;
      skipSpace();
      return TOK_FLOAT;
    } else {
      dst->I = negate ? -ipart : ipart;
      skipSpace();
      return TOK_INT;
    }
  }

  // Read the next word (naked string).
  // If the word is valid, returns start pointer and stores length in len.
  // If invalid, returns a null pointer.
  char* lexWord(int *len) {
    int start = rp_;

    if (!isWordStartChar(curr())) {
      return nullptr;
    }

    adv();

    while (isWordTailChar(curr())) {
      adv();
    }

    *len = rp_ - start;
    return &args_[start];
  }

  int lexDecInt() {
    int i = 0;
    if (!isNumeric(curr())) {
      return -1;
    }
    while (isNumeric(curr())) {
      i *= 10;
      i += (curr() - '0');
      adv();
    }
    return i;
  }

  int lexHexInt() {
    int i = 0;
    if (!isHexit(curr())) {
      return -1;
    }
    while (isHexit(curr())) {
      i <<= 4;
      i |= decodeHexit(curr());
      adv();
    }
    return i;
  }

  void skipSpace() {
    while (true) {
      char ch = curr();
      if (ch == ' ' || ch == '\t') {
        adv();
      } else {
        break;
      }
    }
  }

  inline char curr() { return args_[rp_]; }
  inline char peek() { return args_[rp_+1]; }
  inline void adv() { rp_++; }

  char *args_;
  int len_;
  int rp_;
};

}
