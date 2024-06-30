#pragma once
// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <string>

namespace tty::in {
void Setup();
bool WaitForReady(int timeout_ms = 20);
std::string Read();
void Cleanup();
}  // namespace tty::in
