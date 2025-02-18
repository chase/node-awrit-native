{
  "targets": [
    {
      "target_name": "awrit-native",
      "sources": [
        "tty/escape_parser.cpp",
        "tty/input_posix.cpp",
        "tty/kitty_keys.cpp",
        "tty/sgr_mouse.cpp",
        "string/string_utils.cpp",
        "third_party/utf8_decode.cpp",
        "awrit-native.cpp",
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "tty",
        ".",
      ],
      "variables": {
        'NAPI_VERSION%': "<!(node -p \"process.versions.napi\")",
      },
      "cflags!": [ "-fno-exceptions", "-std=c++17" ],
      "cflags_cc!": [ "-fno-exceptions", "-std=c++17" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "conditions": [
        ['NAPI_VERSION!=""', { 'defines': ['NAPI_VERSION=<@(NAPI_VERSION)'] } ],
        ["OS == 'linux'", {
          "libraries": ["-lrt"]
        }],
        ["OS == 'mac'", {
          "cflags+": ["-fvisibility=hidden", "-std=c++17" ],
          "xcode_settings": {
            # -fvisibility=hidden
            "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",

            # Set minimum target version because we're building on newer
            # Same as https://github.com/nodejs/node/blob/v10.0.0/common.gypi#L416
            "MACOSX_DEPLOYMENT_TARGET": "10.7",

            # Build universal binary to support M1 (Apple silicon)
            "OTHER_CFLAGS": [
              "-arch x86_64",
              "-arch arm64"
            ],
            "OTHER_LDFLAGS": [
              "-arch x86_64",
              "-arch arm64"
            ]
          }
        }],
      ]
    },
  ]
}
