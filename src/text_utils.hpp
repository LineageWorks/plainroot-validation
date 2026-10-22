#ifndef PLAINROOT_VALIDATION_TEXT_UTILS_HPP
#define PLAINROOT_VALIDATION_TEXT_UTILS_HPP

#include <cstddef>
#include <cstdint>

namespace plainroot_validation_detail {

inline bool valid_utf8(const char *data, size_t size,
                       bool reject_nul) noexcept {
  if (size && !data)
    return false;
  const auto *bytes = reinterpret_cast<const unsigned char *>(data);
  size_t i = 0;
  while (i < size) {
    const unsigned char first = bytes[i];
    if (first <= 0x7f) {
      if (reject_nul && first == 0)
        return false;
      ++i;
      continue;
    }

    uint32_t codepoint = 0;
    size_t length = 0;
    if (first >= 0xc2 && first <= 0xdf) {
      codepoint = first & 0x1f;
      length = 2;
    } else if (first >= 0xe0 && first <= 0xef) {
      codepoint = first & 0x0f;
      length = 3;
    } else if (first >= 0xf0 && first <= 0xf4) {
      codepoint = first & 0x07;
      length = 4;
    } else {
      return false;
    }
    if (length > size - i)
      return false;
    for (size_t j = 1; j < length; ++j) {
      const unsigned char continuation = bytes[i + j];
      if ((continuation & 0xc0) != 0x80)
        return false;
      codepoint = (codepoint << 6) | (continuation & 0x3f);
    }
    if ((length == 3 && codepoint < 0x800) ||
        (length == 4 && codepoint < 0x10000) ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
        codepoint > 0x10ffff)
      return false;
    i += length;
  }
  return true;
}

inline bool ascii_whitespace(unsigned char value) noexcept {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

}  // namespace plainroot_validation_detail

#endif
