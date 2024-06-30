#pragma once
// Copyright (c) 2023-2024 Chase Colman. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// simple escape codes
#define ESC "\x1B"
#define CSI ESC "["
#define MODE "?"  // DEC private mode
#define S7C1T ESC " F"
#define SAVE_CURSOR ESC "7"
#define RESTORE_CURSOR ESC "8"
#define SAVE_PRIVATE_MODE_VALUES CSI "?s"
#define RESTORE_PRIVATE_MODE_VALUES CSI "?r"
#define SAVE_COLORS CSI "#P"
#define RESTORE_COLORS CSI "#Q"
#define DECSACE_DEFAULT_REGION_SELECT CSI "*x"
#define CLEAR_SCREEN CSI "H" CSI "2J"
#define RESET_IRM CSI "4l"
