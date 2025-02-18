// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "kitty_keys.h"

#include <charconv>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "escape_codes.h"
#include "string/string_utils.h"

namespace tty::keys {

void Enable() {
  fprintf(stdout, CSI ">%du",
          Flags::DisambiguateEscapeCodes | Flags::ReportEventTypes |
              Flags::ReportAlternateKeys | Flags::ReportAllKeysAsEscapeCodes |
              Flags::ReportAssociatedText);
  fflush(stdout);
}

void Disable() {
  fputs(CSI "<u", stdout);
  fflush(stdout);
}

namespace {

std::u16string_view functional_key_number_to_electron_string(
    uint16_t key_number) {
  static const std::unordered_map<uint16_t, std::u16string_view> map{
      {57344, u"esc"},
      {57345, u"enter"},
      {57346, u"tab"},
      {57347, u"backspace"},
      {57348, u"insert"},
      {57349, u"delete"},
      {57350, u"left"},
      {57351, u"right"},
      {57352, u"up"},
      {57353, u"down"},
      {57354, u"pageup"},
      {57355, u"pagedown"},
      {57356, u"home"},
      {57357, u"end"},
      {57358, u"capslock"},
      {57359, u"scrolllock"},
      {57360, u"numlock"},
      {57361, u"printscreen"},
      {57362, u"pause"},
      {57363, u"menu"},
      {57364, u"f1"},
      {57365, u"f2"},
      {57366, u"f3"},
      {57367, u"f4"},
      {57368, u"f5"},
      {57369, u"f6"},
      {57370, u"f7"},
      {57371, u"f8"},
      {57372, u"f9"},
      {57373, u"f10"},
      {57374, u"f11"},
      {57375, u"f12"},
      {57376, u"f13"},
      {57377, u"f14"},
      {57378, u"f15"},
      {57379, u"f16"},
      {57380, u"f17"},
      {57381, u"f18"},
      {57382, u"f19"},
      {57383, u"f20"},
      {57384, u"f21"},
      {57385, u"f22"},
      {57386, u"f23"},
      {57387, u"f24"},
      {57399, u"num0"},
      {57400, u"num1"},
      {57401, u"num2"},
      {57402, u"num3"},
      {57403, u"num4"},
      {57404, u"num5"},
      {57405, u"num6"},
      {57406, u"num7"},
      {57407, u"num8"},
      {57408, u"num9"},
      {57409, u"numdec"},
      {57410, u"numdiv"},
      {57411, u"nummult"},
      {57412, u"numsub"},
      {57413, u"numadd"},
      {57414, u"return"},
      {57416, u"."},
      {57417, u"left"},
      {57418, u"right"},
      {57419, u"up"},
      {57420, u"down"},
      {57421, u"pageup"},
      {57422, u"pagedown"},
      {57423, u"home"},
      {57424, u"end"},
      {57425, u"insert"},
      {57426, u"delete"},
      {57428, u"mediaplaypause"},
      {57429, u"mediaplaypause"},
      {57430, u"mediaplaypause"},
      {57432, u"mediastop"},
      {57435, u"medianexttrack"},
      {57436, u"mediaprevtrack"},
      {57438, u"volumedown"},
      {57439, u"volumeup"},
      {57440, u"volumemute"},
      {57441, u"left+shift"},
      {57442, u"left+control"},
      {57443, u"left+alt"},
      {57444, u"left+meta"},
      {57445, u"left+meta"},
      {57446, u"left+meta"},
      {57447, u"right+shift"},
      {57448, u"right+control"},
      {57449, u"right+alt"},
      {57450, u"right+meta"},
      {57451, u"right+meta"},
      {57452, u"right+meta"},
  };
  auto result = map.find(key_number);
  return result == map.end() ? u"" : result->second;
}

std::optional<uint16_t> csi_number_to_functional_number(uint16_t csi) {
  static const std::unordered_map<uint16_t, uint16_t> map{
      {2, 57348},   {3, 57349},  {5, 57354},  {6, 57355},  {7, 57356},
      {8, 57357},   {9, 57346},  {11, 57364}, {12, 57365}, {13, 57345},
      {14, 57367},  {15, 57368}, {17, 57369}, {18, 57370}, {19, 57371},
      {20, 57372},  {21, 57373}, {23, 57374}, {24, 57375}, {27, 57344},
      {127, 57347},
  };
  auto result = map.find(csi);
  if (result == map.end())
    return {};

  return result->second;
}

std::optional<uint16_t> letter_trailer_to_csi_number(char trailer) {
  static const std::unordered_map<char, uint16_t> map{
      {'A', 57352}, {'B', 57353}, {'C', 57351}, {'D', 57350}, {'E', 57427},
      {'F', 8},     {'H', 7},     {'P', 11},    {'Q', 12},    {'S', 14},
  };
  auto result = map.find(trailer);
  if (result == map.end())
    return {};

  return result->second;
};

std::vector<int> get_sub_sections(std::string_view section,
                                  int missing) noexcept {
  std::vector<std::string_view> p = string::split(section, ':');
  std::vector<int> result(p.size(), missing);

  for (size_t i = 0; i < p.size(); ++i) {
    if (p[i].empty()) {
      result[i] = missing;
    } else {
      const std::from_chars_result conv =
          std::from_chars(p[i].begin(), p[i].end(), result[i]);
      if (conv.ec != std::errc())
        return {};
    }
  }

  return result;
}

std::u16string modifiers_to_string(Modifiers::Type m) {
  std::u16string result;
  if (m & Modifiers::Meta)
    result += u"meta+";
  if (m & Modifiers::Ctrl)
    result += u"ctrl+";
  if (m & Modifiers::Shift)
    result += u"shift+";
  if (m & Modifiers::Alt)
    result += u"alt+";
  if (m & Modifiers::CapsLock)
    result += u"capslock+";
  if (m & Modifiers::NumLock)
    result += u"numlock+";
  return result;
}

}  // namespace

std::pair<Event::Type, std::u16string> ElectronKeyEventFromCSI(
    std::string_view csi) noexcept {
  std::u16string keyCode;
  std::u16string modifiers;
  Event::Type event(Event::Invalid);
  if (csi.empty()) {
    return {event, keyCode};
  }

  char last_char = csi.back();
  csi.remove_suffix(1);

  static constexpr std::string_view possible_trailers{"u~ABCDEHFPQRS"};

  if (possible_trailers.find(last_char) == std::string_view::npos ||
      (last_char == '~' && (csi == "200" || csi == "201"))) {
    return {event, keyCode};
  }

  std::vector<std::string_view> sections = string::split(csi, ';');
  std::vector<int> first_section;
  if (sections.size() > 0)
    first_section = get_sub_sections(sections[0], 0);
  std::vector<int> second_section;
  if (sections.size() > 1)
    second_section = get_sub_sections(sections[1], 1);
  std::vector<int> third_section;
  if (sections.size() > 2)
    third_section = get_sub_sections(sections[2], 0);

  uint32_t keynum = 0;
  auto maybe_csi_number = letter_trailer_to_csi_number(last_char);
  if (maybe_csi_number) {
    keynum = maybe_csi_number.value();
  } else {
    if (first_section.empty())
      return {event, keyCode};
    keynum = first_section[0];
  }

  if (second_section.size() > 0) {
    modifiers += modifiers_to_string(
        static_cast<Modifiers::Type>(second_section[0] - 1));
  }

  if (second_section.size() > 1) {
    event = static_cast<Event::Type>(second_section[1]);
    if (event == Event::Repeat) {
      modifiers += u"isautorepeat+";
    }
  } else {
    event = Event::Down;
  }

  if (keynum == 13) {
    keyCode = last_char == 'u' ? u"enter" : u"f3";
  } else if (keynum != 0) {
    auto functional_number = csi_number_to_functional_number(keynum);
    if (functional_number) {
      keynum = functional_number.value();
    }
    keyCode = functional_key_number_to_electron_string(keynum);
  }

  if (keyCode.empty()) {
    // Electron only supports function keys and the US key layout
    if (keynum >= ' ' && keynum <= '~') {
      keyCode.push_back(keynum);
    } else {
      // So if it's not ASCII or a function key, check if we can pass off
      // codepoints (usually on key up)
      if (third_section.size() > 0) {
        event = Event::Unicode;
        for (auto codepoint : third_section)
          keyCode.push_back(codepoint);
      } else {
        event = Event::Invalid;
      }
    }
  }

  if (event == Event::Repeat) {
    event = Event::Down;
  }

  return {event, modifiers + keyCode};
}
}  // namespace tty::keys
