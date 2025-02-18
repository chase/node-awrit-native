// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef AWRIT_TTY_ESCAPE_PARSER_H
#define AWRIT_TTY_ESCAPE_PARSER_H

#include <cstdint>
#include <string>
#include <string_view>

#include "third_party/utf8_decode.h"

namespace tty {

namespace csi {
enum class State { Parameter, Intermediate };
enum class Char { Unknown, Parameter, Intermediate, Final };
}  // namespace csi

class EscapeCodeParser {
 public:
  EscapeCodeParser() { Reset(); }

  bool Parse(std::string_view buffer);

  enum class Type : int {
    None = 0,
    CSI = 1,
    OSC = 2,
    DCS = 3,
    PM = 4,
    SOS = 5,
    APC = 6,
    Key = 7,
    Mouse = 8,
    Unicode = 9,
  };

 protected:
  virtual bool HandleUTF8Codepoint(uint32_t) { return true; };
  virtual bool Handle(Type type, const std::string&) { return true; };

 private:
  enum class State {
    Normal,
    ESC,
    CSI,
    ST,
    ST_or_BEL,
    ESC_ST,
    C1_ST,
  };

  State state_;
  utf8::State utf8_state_;
  uint32_t utf8_codepoint_;
  csi::State csi_state_;
  std::string buffer_;
  Type handler_;

  bool Parse(char ch);
  bool Reset();

  bool Byte(uint8_t ch);
  bool UTF8Codepoint(uint32_t ch);
  void Invalid() { Reset(); };
  bool EscapeCode();

  bool ESC(uint8_t ch);
  bool CSI(uint8_t ch);
  bool ST(uint8_t ch);
  bool ESC_ST(uint8_t ch);
  bool C1_ST(uint8_t ch);
};

}  // namespace tty

#endif  // AWRIT_TTY_ESCAPE_PARSER_H
