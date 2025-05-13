#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#include "zap_helpers.hpp"
#include "zap_arg_parser.hpp"
#include "zap_string_table.hpp"

#define ZAP_PARSE_ARGS(str, len) ZAP_PARSE_ARGS_EX(args, arg, str, len)

#define ZAP_PARSE_ARGS_EX(parserVar, argVar, str, len) \
  zap::ArgParser parserVar(str, len);                  \
  zap::Arg argVar

namespace zap {

extern const uint8_t FRAME_TYPE_BINARY;
extern const uint8_t FRAME_TYPE_TEXT;

class Stream;

};  // namespace zap

#include "zap_protocol.hpp"
#include "zap_stream.hpp"