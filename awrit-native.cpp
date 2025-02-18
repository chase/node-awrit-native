#include <fcntl.h>
#include <napi.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

#include "escape_parser.h"
#include "input.h"
#include "kitty_keys.h"
#include "sgr_mouse.h"

using namespace Napi;
Value ShmWrite(const CallbackInfo &info)
{
  Env env = info.Env();

  if (info.Length() < 2 || info.Length() > 3 || !info[0].IsString() || !info[1].IsBuffer())
  {
    TypeError::New(env, "Expected a string and a buffer and optionally a boolean")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<String>().Utf8Value();
  Buffer<char> buffer = info[1].As<Buffer<char>>();
  bool rgbaFix = info[2].IsBoolean() ? info[2].As<Boolean>().Value() : false;

  int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd == -1)
  {
    Error::New(env, "Failed to open shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (ftruncate(fd, buffer.Length()) == -1)
  {
    close(fd);
    Error::New(env, "Failed to set size of shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  void *ptr = mmap(0, buffer.Length(), PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED)
  {
    close(fd);
    Error::New(env, "Failed to map shared memory").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (rgbaFix)
  {
    const char *src = buffer.Data();
    size_t len = buffer.Length();

#if defined(__AVX2__)
    const __m256i shuffle_mask = _mm256_set_epi8(31, 28, 29, 30, 27, 24, 25, 26, 23, 20, 21, 22, 19, 16, 17, 18,
                                                 15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
    for (size_t i = 0; i + 31 < len; i += 32)
    {
      __m256i pixels = _mm256_loadu_si256((__m256i *)(src + i));
      __m256i shuffled = _mm256_shuffle_epi8(pixels, shuffle_mask);
      _mm256_storeu_si256((__m256i *)((char *)ptr + i), shuffled);
    }
#elif defined(__SSSE3__)
    const __m128i shuffle_mask = _mm_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2);
    for (size_t i = 0; i + 15 < len; i += 16)
    {
      __m128i pixels = _mm_loadu_si128((__m128i *)(src + i));
      __m128i shuffled = _mm_shuffle_epi8(pixels, shuffle_mask);
      _mm_storeu_si128((__m128i *)((char *)ptr + i), shuffled);
    }
#elif defined(__ARM_NEON)
    const uint8x16_t shuffle_mask = {2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15};
    for (size_t i = 0; i + 15 < len; i += 16)
    {
      uint8x16_t pixels = vld1q_u8((uint8_t *)(src + i));
      uint8x16_t shuffled = vqtbl1q_u8(pixels, shuffle_mask);
      vst1q_u8((uint8_t *)((char *)ptr + i), shuffled);
    }
#else
    // Fallback scalar implementation
    for (size_t i = 0; i + 3 < len; i += 4)
    {
      ((char *)ptr)[i] = src[i + 2];     // R
      ((char *)ptr)[i + 1] = src[i + 1]; // G
      ((char *)ptr)[i + 2] = src[i];     // B
      ((char *)ptr)[i + 3] = src[i + 3]; // A
    }
#endif
  }
  else
  {
    std::memcpy(ptr, buffer.Data(), buffer.Length());
  }

  munmap(ptr, buffer.Length());
  close(fd);

  return env.Undefined();
}

Value ShmUnlink(const CallbackInfo &info)
{
  Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsString())
  {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<String>().Utf8Value();
  if (shm_unlink(name.c_str()) == -1 && errno != ENOENT)
  {
    Error::New(env, "Failed to unlink shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

Value SetupInput(const CallbackInfo &info)
{
  Env env = info.Env();
  tty::in::Setup();
  tty::keys::Enable();
  return env.Undefined();
}

Value CleanupInput(const CallbackInfo &info)
{
  Env env = info.Env();
  tty::keys::Disable();
  tty::in::Cleanup();
  return env.Undefined();
}

static std::atomic<bool> s_quit = false;

struct Event
{
  Event(tty::EscapeCodeParser::Type type_, const std::string &string_)
      : type(type_), string(string_) {}
  const tty::EscapeCodeParser::Type type;
  const std::string string;
};

class InputEventParser final : public tty::EscapeCodeParser
{
private:
  static Object HandleMouse(Env env, const tty::mouse::MouseEvent &event)
  {
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

  static Object HandleCSI(Env env, const std::string &csi)
  {
    auto obj = Object::New(env);

    const auto &[keyEvent, keyString] = tty::keys::ElectronKeyEventFromCSI(csi);
    if (keyEvent != tty::keys::Event::Invalid)
    {
      obj["type"] =
          Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::Key));
      obj["event"] = Number::New(env, keyEvent);
      obj["code"] = String::New(env, keyString);
      return obj;
    }

    const auto mc = tty::sgr_mouse::MouseEventFromCSI(csi);
    if (mc)
    {
      return HandleMouse(env, *mc);
    }

    obj["type"] =
        Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::CSI));
    obj["data"] = String::New(env, csi);
    return obj;
  };

  static void Callback(Env env, Function callback, void *, Event *event)
  {
    using Type = tty::EscapeCodeParser::Type;
    if (env != nullptr && callback != nullptr && event != nullptr &&
        event->type != Type::None)
    {
      if (event->type == Type::Unicode)
      {
        auto obj = Object::New(env);
        obj["type"] = Number::New(env, static_cast<int>(Type::Key));
        obj["event"] = Number::New(env, static_cast<int>(tty::keys::Event::Unicode));
        obj["code"] = String::New(env, event->string);
        callback.Call({obj});
      }
      else if (event->type != Type::CSI)
      {
        auto obj = Object::New(env);
        obj["type"] = Number::New(env, static_cast<int>(event->type));
        obj["data"] = String::New(env, event->string);
        callback.Call({obj});
      }
      else
      {
        callback.Call({HandleCSI(env, event->string)});
      }
    }

    if (event != nullptr)
      delete event;
  }

  using TSFN = TypedThreadSafeFunction<void, Event, InputEventParser::Callback>;
  TSFN callback_;

protected:
  bool Handle(Type type, const std::string &data) override
  {
    callback_.BlockingCall(new Event(type, data));
    return true;
  };

  bool HandleUTF8Codepoint(uint32_t codepoint) override
  {
    std::string result;
    if (codepoint <= 0x7F)
    {
      // Handle ASCII characters (0-127)
      result.push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FF)
    {
      // Handle 2-byte sequence (128-2047)
      result.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0xFFFF)
    {
      // Handle 3-byte sequence (2048-65535)
      result.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0x10FFFF)
    {
      // Handle 4-byte sequence (65536-1114111)
      result.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    callback_.BlockingCall(new Event(tty::EscapeCodeParser::Type::Unicode, result));
    return true;
  }

public:
  explicit InputEventParser(const CallbackInfo &info)
  {
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

Value ListenForInput(const CallbackInfo &info)
{
  Env env = info.Env();
  int wait = 10;
  if (info.Length() > 1 && info[1].IsNumber())
  {
    wait = info[1].As<Number>().Int32Value();
    return env.Undefined();
  }
  auto *parser = new InputEventParser(info);
  std::thread([wait, parser]()
              {
                while (!s_quit)
                {
                  if (!tty::in::WaitForReady(wait))
                  {
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                    continue;
                  }
                  parser->Parse(tty::in::Read());
                }
                // delete parser;
              })
      .detach();
  return Napi::Function::New(env, [](const CallbackInfo &)
                             { s_quit = true; });
}

Object Init(Env env, Object exports)
{
  exports.Set(String::New(env, "shmWrite"), Function::New(env, ShmWrite));
  exports.Set(String::New(env, "shmUnlink"), Function::New(env, ShmUnlink));
  exports.Set(String::New(env, "setupInput"), Function::New(env, SetupInput));
  exports.Set(String::New(env, "cleanupInput"),
              Function::New(env, CleanupInput));
  exports.Set(String::New(env, "listenForInput"),
              Function::New(env, ListenForInput));
  return exports;
}

NODE_API_MODULE(awritnative, Init)
