#pragma once
// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <optional>
#include <string_view>

#include "mouse.h"

namespace tty::sgr_mouse {

void Enable();
std::optional<mouse::MouseEvent> MouseEventFromCSI(std::string_view csi) noexcept;

}  // namespace tty::sgr_mouse
