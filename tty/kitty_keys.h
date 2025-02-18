#pragma once
// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <cstdint>
#include <string>
#include <string_view>

// see https://sw.kovidgoyal.net/kitty/keyboard-protocol/
namespace tty::keys {

namespace Modifiers {
enum Type : int {
  Shift = 1 << 0,
  Alt = 1 << 1,
  Ctrl = 1 << 2,
  Super = 1 << 3,
  Hyper = 1 << 4,
  Meta = 1 << 5,
  CapsLock = 1 << 6,
  NumLock = 1 << 7,

  None = 0
};

}  // namespace Modifiers

namespace Event {
enum Type : int { Invalid = 0, Down = 1, Repeat = 2, Up = 3, Unicode = 4 };
}

namespace Flags {
enum Type : int {
  DisambiguateEscapeCodes = 1,
  ReportEventTypes = 2,
  ReportAlternateKeys = 4,
  ReportAllKeysAsEscapeCodes = 8,
  ReportAssociatedText = 16,
  None = 0
};
}

void Enable();
void Disable();

std::pair<Event::Type, std::u16string> ElectronKeyEventFromCSI(
    std::string_view csi) noexcept;

}  // namespace tty::keys
