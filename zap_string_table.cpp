#include "Zap.hpp"

namespace zap {

#define ZAP_STRING(name, ident, str) const char str_contents_##name[] PROGMEM = str;
#include "zap_string_table.x.hpp"
#undef ZAP_STRING

#define ZAP_STRING(name, ident, str) str_contents_##name,
const char *const string_table[] PROGMEM = {
#include "zap_string_table.x.hpp"
};
#undef ZAP_STRING

};  // namespace zap
