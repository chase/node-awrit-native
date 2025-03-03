#include <fcntl.h>
#include <napi.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

#if defined(__AVX2__)
constexpr size_t ALIGNMENT = 32;  // AVX2 alignment
#elif defined(__ARM_NEON)
constexpr size_t ALIGNMENT = 16;  // NEON alignment
#else
constexpr size_t ALIGNMENT = 4;  // Default alignment
#endif

#include "escape_parser.h"
#include "input.h"
#include "kitty_keys.h"
#include "sgr_mouse.h"

#define BYTES_PER_PIXEL 4

using namespace Napi;

// Helper function to align size to the required SIMD alignment
constexpr size_t align_size(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

class ShmGraphicBuffer : public ObjectWrap<ShmGraphicBuffer> {
 public:
  static Object Init(Napi::Env env, Object exports) {
    Function func =
        DefineClass(env, "ShmGraphicBuffer",
                    {InstanceMethod("write", &ShmGraphicBuffer::Write)});

    FunctionReference* constructor = new FunctionReference();
    *constructor = Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("ShmGraphicBuffer", func);
    return exports;
  }

  ShmGraphicBuffer(const CallbackInfo& info)
      : ObjectWrap<ShmGraphicBuffer>(info) {
    Napi::Env env = info.Env();

    if (info.Length() != 1 || !info[0].IsString()) {
      TypeError::New(env, "Expected a string and two numbers")
          .ThrowAsJavaScriptException();
      return;
    }

    name = info[0].As<String>().Utf8Value();
  }

  ~ShmGraphicBuffer() {
    if (fd != -1) {
      close(fd);
      fd = -1;
      shm_unlink(name.c_str());
    }
  }

 private:
  Napi::Value Write(const CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsObject()) {
      TypeError::New(env,
                     "Expected a buffer, sourceSize, and optionally a destRect")
          .ThrowAsJavaScriptException();
      return env.Undefined();
    }

    fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
      Error::New(env, "Failed to open shared memory")
          .ThrowAsJavaScriptException();
    }

    Buffer<char> buffer = info[0].As<Buffer<char>>();
    Object sourceSize = info[1].As<Object>();
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;

    if (sourceSize.Has("width") && sourceSize.Get("width").IsNumber()) {
      sourceWidth = sourceSize.Get("width").As<Number>().Uint32Value();
    }
    if (sourceSize.Has("height") && sourceSize.Get("height").IsNumber()) {
      sourceHeight = sourceSize.Get("height").As<Number>().Uint32Value();
    }
    size_t alignedSize =
        align_size(sourceWidth * sourceHeight * BYTES_PER_PIXEL, ALIGNMENT);

    if (alignedSize != lastAlignedSize && ftruncate(fd, alignedSize) == -1) {
      close(fd);
      Error::New(env, "Failed to resize shared memory")
          .ThrowAsJavaScriptException();
      return env.Undefined();
    }

    // Default dirty region is the entire buffer
    uint32_t dirtyX = 0;
    uint32_t dirtyY = 0;
    uint32_t dirtyWidth = sourceWidth;
    uint32_t dirtyHeight = sourceHeight;

    // Check for dirty rectangle parameter
    if (info.Length() > 2 && info[2].IsObject()) {
      Object dirtyRect = info[2].As<Object>();

      if (dirtyRect.Has("x") && dirtyRect.Get("x").IsNumber()) {
        dirtyX = dirtyRect.Get("x").As<Number>().Uint32Value();
      }

      if (dirtyRect.Has("y") && dirtyRect.Get("y").IsNumber()) {
        dirtyY = dirtyRect.Get("y").As<Number>().Uint32Value();
      }

      if (dirtyRect.Has("width") && dirtyRect.Get("width").IsNumber()) {
        dirtyWidth = dirtyRect.Get("width").As<Number>().Uint32Value();
      }

      if (dirtyRect.Has("height") && dirtyRect.Get("height").IsNumber()) {
        dirtyHeight = dirtyRect.Get("height").As<Napi::Number>().Uint32Value();
      }

      // Ensure dirty region is within bounds
      if (dirtyX >= sourceWidth)
        dirtyX = 0;
      if (dirtyY >= sourceHeight)
        dirtyY = 0;
      if (dirtyX + dirtyWidth > sourceWidth)
        dirtyWidth = sourceWidth - dirtyX;
      if (dirtyY + dirtyHeight > sourceHeight)
        dirtyHeight = sourceHeight - dirtyY;
    }

    // Map the shared memory
    void* ptr = mmap(0, alignedSize, PROT_WRITE, MAP_SHARED, this->fd, 0);
    if (ptr == MAP_FAILED) {
      Napi::Error::New(env, "Failed to map shared memory")
          .ThrowAsJavaScriptException();
      return env.Undefined();
    }

    // Apply RGBA fix (swap R and B channels) only for the dirty region
    const char* src = buffer.Data();
    char* dst = (char*)ptr;

    // Calculate offsets and strides
    size_t rowStride = sourceWidth * BYTES_PER_PIXEL;
    size_t dirtyOffset = dirtyX * BYTES_PER_PIXEL;

    // Align the dirty region width to the appropriate SIMD boundary
    // This ensures we always process complete SIMD blocks
    size_t dirtyRowSize = align_size(dirtyWidth * BYTES_PER_PIXEL, ALIGNMENT);

    // Process each row in the dirty region
    for (uint32_t y = 0; y < dirtyHeight; y++) {
      size_t rowOffset = dirtyOffset + y * rowStride;

#if defined(__AVX2__)
      const __m256i shuffle_mask = _mm256_set_epi8(
          31, 28, 29, 30, 27, 24, 25, 26, 23, 20, 21, 22, 19, 16, 17, 18, 15,
          12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
      for (size_t i = rowOffset; i < rowOffset + dirtyRowSize; i += 32) {
        __m256i pixels = _mm256_loadu_si256((__m256i*)(src + i));
        __m256i shuffled = _mm256_shuffle_epi8(pixels, shuffle_mask);
        _mm256_storeu_si256((__m256i*)(dst + i), shuffled);
      }
#elif defined(__ARM_NEON)
      const uint8x16_t shuffle_mask = {2,  1, 0, 3,  6,  5,  4,  7,
                                       10, 9, 8, 11, 14, 13, 12, 15};
      for (size_t i = rowOffset; i < rowOffset + dirtyRowSize; i += 16) {
        uint8x16_t pixels = vld1q_u8((uint8_t*)(src + i));
        uint8x16_t shuffled = vqtbl1q_u8(pixels, shuffle_mask);
        vst1q_u8((uint8_t*)(dst + i), shuffled);
      }
#else
      // Fallback scalar implementation for the dirty region
      for (size_t i = rowOffset; i < rowOffset + dirtyRowSize; i += 4) {
        dst[i] = src[i + 2];      // R
        dst[i + 1] = src[i + 1];  // G
        dst[i + 2] = src[i];      // B
        dst[i + 3] = src[i + 3];  // A
      }
#endif
    }

    // Unmap the shared memory
    munmap(ptr, alignedSize);
    if (fd != -1) {
      close(fd);
      fd = -1;
    }

    // Create and return a Rect object with the dirty rectangle information
    Object result = Object::New(env);
    result["x"] = Number::New(env, dirtyX);
    result["y"] = Number::New(env, dirtyY);
    result["width"] = Number::New(env, dirtyRowSize / BYTES_PER_PIXEL);
    result["height"] = Number::New(env, dirtyHeight);

    return result;
  }

  std::string name;
  int fd;
  size_t lastAlignedSize;
};

Value SetupInput(const CallbackInfo& info) {
  Env env = info.Env();
  tty::in::Setup();
  tty::keys::Enable();
  return env.Undefined();
}

Value CleanupInput(const CallbackInfo& info) {
  Env env = info.Env();
  tty::keys::Disable();
  tty::in::Cleanup();
  return env.Undefined();
}

static std::atomic<bool> s_quit = false;

struct Event {
  Event(tty::EscapeCodeParser::Type type_, const std::string& string_)
      : type(type_), string(string_) {}
  const tty::EscapeCodeParser::Type type;
  const std::string string;
};

class InputEventParser final : public tty::EscapeCodeParser {
 private:
  static Object HandleMouse(Env env, const tty::mouse::MouseEvent& event) {
    auto obj = Object::New(env);
    obj["type"] =
        Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::Mouse));
    obj["event"] = Number::New(env, event.type);
    obj["buttons"] = Number::New(env, event.buttons);
    obj["modifiers"] = Number::New(env, event.modifiers);
    if (event.x > -1)
      obj["x"] = Number::New(env, event.x);
    if (event.y > -1)
      obj["y"] = Number::New(env, event.y);

    return obj;
  }

  static Object HandleCSI(Env env, const std::string& csi) {
    auto obj = Object::New(env);

    const auto& [keyEvent, keyString] = tty::keys::ElectronKeyEventFromCSI(csi);
    if (keyEvent != tty::keys::Event::Invalid) {
      obj["type"] =
          Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::Key));
      obj["event"] = Number::New(env, keyEvent);
      auto modifiers = Array::New(env);
      for (size_t index = 0; index < keyString.size() - 1; ++index) {
        modifiers.Set(index, String::New(env, keyString[index]));
      }
      obj["modifiers"] = modifiers;
      obj["code"] = String::New(env, keyString[keyString.size() - 1]);
      return obj;
    }

    const auto mc = tty::sgr_mouse::MouseEventFromCSI(csi);
    if (mc) {
      return HandleMouse(env, *mc);
    }

    obj["type"] =
        Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::CSI));
    obj["data"] = String::New(env, csi);
    return obj;
  };

  static void Callback(Env env, Function callback, void*, Event* event) {
    using Type = tty::EscapeCodeParser::Type;
    if (env != nullptr && callback != nullptr && event != nullptr &&
        event->type != Type::None) {
      if (event->type == Type::Unicode) {
        auto obj = Object::New(env);
        obj["type"] = Number::New(env, static_cast<int>(Type::Key));
        obj["event"] =
            Number::New(env, static_cast<int>(tty::keys::Event::Unicode));
        obj["code"] = String::New(env, event->string);
        callback.Call({obj});
      } else if (event->type != Type::CSI) {
        auto obj = Object::New(env);
        obj["type"] = Number::New(env, static_cast<int>(event->type));
        obj["data"] = String::New(env, event->string);
        callback.Call({obj});
      } else {
        callback.Call({HandleCSI(env, event->string)});
      }
    }

    if (event != nullptr)
      delete event;
  }

  using TSFN = TypedThreadSafeFunction<void, Event, InputEventParser::Callback>;
  TSFN callback_;

 protected:
  bool Handle(Type type, const std::string& data) override {
    callback_.BlockingCall(new Event(type, data));
    return true;
  };

  bool HandleUTF8Codepoint(uint32_t codepoint) override {
    std::string result;
    if (codepoint <= 0x7F) {
      // Handle ASCII characters (0-127)
      result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      // Handle 2-byte sequence (128-2047)
      result.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      // Handle 3-byte sequence (2048-65535)
      result.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
      // Handle 4-byte sequence (65536-1114111)
      result.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    callback_.BlockingCall(
        new Event(tty::EscapeCodeParser::Type::Unicode, result));
    return true;
  }

 public:
  explicit InputEventParser(const CallbackInfo& info) {
    // clang-format off
    callback_ = TSFN::New(
				info.Env(),
				info[0].As<Function>(),
				Object::New(info.Env()),
				"InputParserCallback",
				0,
				1
		);
    // clang-format on
  }
};

Value ListenForInput(const CallbackInfo& info) {
  Env env = info.Env();
  int wait = 10;
  if (info.Length() > 1 && info[1].IsNumber()) {
    wait = info[1].As<Number>().Int32Value();
    return env.Undefined();
  }
  auto* parser = new InputEventParser(info);
  std::thread([wait, parser]() {
    while (!s_quit) {
      if (!tty::in::WaitForReady(wait)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        continue;
      }
      parser->Parse(tty::in::Read());
    }
    // delete parser;
  }).detach();
  return Napi::Function::New(env, [](const CallbackInfo&) { s_quit = true; });
}

Object Init(Env env, Object exports) {
  // Initialize the ShmGraphicBuffer class
  ShmGraphicBuffer::Init(env, exports);

  exports.Set(String::New(env, "setupInput"), Function::New(env, SetupInput));
  exports.Set(String::New(env, "cleanupInput"),
              Function::New(env, CleanupInput));
  exports.Set(String::New(env, "listenForInput"),
              Function::New(env, ListenForInput));
  return exports;
}

NODE_API_MODULE(awritnative, Init)
