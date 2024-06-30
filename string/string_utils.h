#pragma once
// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <optional>
#include <string_view>
#include <vector>

namespace string {
std::vector<std::string_view> split(const std::string_view& str,
                                    char delimiter);
std::optional<int> strtoint(std::string_view str);
}  // namespace string
