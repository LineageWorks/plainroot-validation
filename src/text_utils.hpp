#ifndef PLAINROOT_VALIDATION_TEXT_UTILS_HPP
#define PLAINROOT_VALIDATION_TEXT_UTILS_HPP

#include <cstddef>
#include <cstdint>

namespace plainroot_validation_detail {

inline bool decode_utf8(const unsigned char *bytes, size_t size, size_t& offset,
                        uint32_t& codepoint) noexcept {
  if (offset >= size)
    return false;
  const size_t start = offset;
  const unsigned char first = bytes[start];
  if (first <= 0x7f) {
    codepoint = first;
    offset = start + 1;
    return true;
  }

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
  if (length > size - start)
    return false;
  for (size_t i = 1; i < length; ++i) {
    const unsigned char continuation = bytes[start + i];
    if ((continuation & 0xc0) != 0x80)
      return false;
    codepoint = (codepoint << 6) | (continuation & 0x3f);
  }
  if ((length == 3 && codepoint < 0x800) ||
      (length == 4 && codepoint < 0x10000) ||
      (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
      codepoint > 0x10ffff)
    return false;
  offset = start + length;
  return true;
}

inline bool valid_utf8(const char *data, size_t size, bool reject_nul,
                       size_t *invalid_offset = nullptr) noexcept {
  if (size && !data) {
    if (invalid_offset)
      *invalid_offset = 0;
    return false;
  }
  const auto *bytes = reinterpret_cast<const unsigned char *>(data);
  size_t offset = 0;
  while (offset < size) {
    const size_t start = offset;
    uint32_t codepoint = 0;
    if (!decode_utf8(bytes, size, offset, codepoint) ||
        (reject_nul && codepoint == 0)) {
      if (invalid_offset)
        *invalid_offset = start;
      return false;
    }
  }
  if (invalid_offset)
    *invalid_offset = size;
  return true;
}

inline bool count_utf8_scalars(const char *data, size_t size,
                               uint64_t& count) noexcept {
  count = 0;
  if (size && !data)
    return false;
  const auto *bytes = reinterpret_cast<const unsigned char *>(data);
  size_t offset = 0;
  while (offset < size) {
    uint32_t codepoint = 0;
    if (!decode_utf8(bytes, size, offset, codepoint))
      return false;
    ++count;
  }
  return true;
}

inline bool unicode_white_space(uint32_t codepoint) noexcept {
  /* Unicode 17.0.0 UCD PropList.txt, White_Space (25 code points). */
  return (codepoint >= 0x0009 && codepoint <= 0x000d) ||
         codepoint == 0x0020 || codepoint == 0x0085 || codepoint == 0x00a0 ||
         codepoint == 0x1680 ||
         (codepoint >= 0x2000 && codepoint <= 0x200a) ||
         codepoint == 0x2028 || codepoint == 0x2029 || codepoint == 0x202f ||
         codepoint == 0x205f || codepoint == 0x3000;
}

inline bool contains_non_whitespace_scalar(const char *data,
                                           size_t size) noexcept {
  const auto *bytes = reinterpret_cast<const unsigned char *>(data);
  size_t offset = 0;
  while (offset < size) {
    uint32_t codepoint = 0;
    if (!decode_utf8(bytes, size, offset, codepoint) ||
        !unicode_white_space(codepoint))
      return true;
  }
  return false;
}

inline bool ascii_whitespace(unsigned char value) noexcept {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

}  // namespace plainroot_validation_detail

#endif
