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

#include "escape_parser.h"
#include "input.h"
#include "kitty_keys.h"
#include "sgr_mouse.h"

using namespace Napi;
Value ShmWrite(const CallbackInfo& info) {
  Env env = info.Env();

  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsBuffer()) {
    TypeError::New(env, "Expected a string and a buffer")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<String>().Utf8Value();
  Buffer<char> buffer = info[1].As<Buffer<char>>();

  int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd == -1) {
    Error::New(env, "Failed to open shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (ftruncate(fd, buffer.Length()) == -1) {
    close(fd);
    Error::New(env, "Failed to set size of shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  void* ptr = mmap(0, buffer.Length(), PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    close(fd);
    Error::New(env, "Failed to map shared memory").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::memcpy(ptr, buffer.Data(), buffer.Length());
  munmap(ptr, buffer.Length());
  close(fd);

  return env.Undefined();
}

Value ShmUnlink(const CallbackInfo& info) {
  Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsString()) {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<String>().Utf8Value();
  if (shm_unlink(name.c_str()) == -1 && errno != ENOENT) {
    Error::New(env, "Failed to unlink shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

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
    if (event.x > -1) obj["x"] = Number::New(env, event.x);
    if (event.y > -1) obj["y"] = Number::New(env, event.y);

    return obj;
  }

  static Object HandleCSI(Env env, const std::string& csi) {
    auto obj = Object::New(env);

    const auto& [keyEvent, keyString] = tty::keys::ElectronKeyEventFromCSI(csi);
    if (keyEvent != tty::keys::Event::Invalid) {
      obj["type"] =
          Number::New(env, static_cast<int>(tty::EscapeCodeParser::Type::Key));
      obj["event"] = Number::New(env, keyEvent);
      obj["code"] = String::New(env, keyString);
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
      if (event->type != Type::CSI) {
        auto obj = Object::New(env);
        obj["type"] = Number::New(env, static_cast<int>(event->type));
        obj["data"] = String::New(env, event->string);
        callback.Call({obj});
      } else {
        callback.Call({HandleCSI(env, event->string)});
      }
    }

    if (event != nullptr) delete event;
  }

  using TSFN = TypedThreadSafeFunction<void, Event, InputEventParser::Callback>;
  TSFN callback_;

 protected:
  bool Handle(Type type, const std::string& data) override {
    callback_.BlockingCall(new Event(type, data));
    return true;
  };

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
		delete parser;
  }).detach();
  return Napi::Function::New(env, [](const CallbackInfo&) { s_quit = true; });
}

Object Init(Env env, Object exports) {
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
