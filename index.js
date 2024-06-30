module.exports = require("node-gyp-build")(__dirname);
module.exports.KeyEvent = {
  Invalid: 0,
  Down: 1,
  Repeat: 2,
  Up: 3,
  Unicode: 4,
};
module.exports.MouseEvent = {
  Down: 0,
  Up: 1,
  Move: 2,
};
module.exports.MouseButton = {
  Left: 1 << 0,
  Middle: 1 << 1,
  Right: 1 << 2,
  Fourth: 1 << 3,
  Fifth: 1 << 4,
  Sixth: 1 << 5,
  Seventh: 1 << 6,
  WheelUp: 1 << 7,
  WheelDown: 1 << 8,
  WheelLeft: 1 << 9,
  WheelRight: 1 << 10,
  None: 0,
};
module.exports.MouseModifier = {
  Shift: 1 << 2,
  Alt: 1 << 3,
  Ctrl: 1 << 4,
  Motion: 1 << 5,
  None: 0,
};
module.exports.EscapeType = {
  None: 0,
  CSI: 1,
  OSC: 2,
  DCS: 3,
  PM: 4,
  SOS: 5,
  APC: 6,
  Key: 7,
  Mouse: 8,
};
