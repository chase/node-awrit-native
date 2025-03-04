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
          "libraries": ["-lrt"],
          "cflags+": ["-mavx2", "-mssse3", "-std=c++17"],
          "cflags_cc+": ["-mavx2", "-mssse3", "-std=c++17"]
        }],
        ["OS == 'mac'", {
          "cflags+": ["-fvisibility=hidden", "-std=c++17" ],
          "xcode_settings": {
            # -fvisibility=hidden
            "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
            'ALWAYS_SEARCH_USER_PATHS': 'NO',
            'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
            'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                      # (Equivalent to -fPIC)
            'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
            'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
            'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
            'PREBINDING': 'NO',                       # No -Wl,-prebind

            # Set minimum target version because we're building on newer
            # Same as https://github.com/nodejs/node/blob/v20.18.3/common.gypi#L597
            "MACOSX_DEPLOYMENT_TARGET": "10.15",
            'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
            'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++17',  # -std=gnu++17
            'CLANG_CXX_LIBRARY': 'libc++',

            # Build universal binary to support M1 (Apple silicon)
            "OTHER_CFLAGS": [
              "-arch x86_64", "-mavx2", "-mssse3",
              "-arch arm64", "-ftree-vectorize"
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
