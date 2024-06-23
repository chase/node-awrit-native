#include <fcntl.h>
#include <napi.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

Napi::Value ShmWrite(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsBuffer()) {
    Napi::TypeError::New(env, "Expected a string and a buffer")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  Napi::Buffer<char> buffer = info[1].As<Napi::Buffer<char>>();

  int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    Napi::Error::New(env, "Failed to open shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  if (ftruncate(fd, buffer.Length()) == -1) {
    close(fd);
    Napi::Error::New(env, "Failed to set size of shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  void* ptr = mmap(0, buffer.Length(), PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    close(fd);
    Napi::Error::New(env, "Failed to map shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::memcpy(ptr, buffer.Data(), buffer.Length());
  munmap(ptr, buffer.Length());
  close(fd);

  return env.Undefined();
}

Napi::Value ShmUnlink(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string name = info[0].As<Napi::String>().Utf8Value();
  if (shm_unlink(name.c_str()) == -1) {
    Napi::Error::New(env, "Failed to unlink shared memory")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return env.Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "shmWrite"),
              Napi::Function::New(env, ShmWrite));
  exports.Set(Napi::String::New(env, "shmUnlink"),
              Napi::Function::New(env, ShmUnlink));
  return exports;
}

NODE_API_MODULE(shm, Init)
