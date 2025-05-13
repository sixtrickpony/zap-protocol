#pragma once

#include <avr/pgmspace.h>

namespace zap {

// Define STR_ constants which are offsets into the string table.
#define ZAP_STRING(name, ident, str) STR_##ident,
enum {
#include "zap_string_table.x.hpp"
};
#undef ZAP_STRING

extern const char *const string_table[] PROGMEM;

};  // namespace zap
