#include "product_haptics_backend.h"

#include <SDL3/SDL_system.h>
#include <jni.h>

namespace human_resources::platform::detail {
namespace {
JNIEnv* environment() noexcept { return static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv()); }
bool failed(JNIEnv* env) noexcept {
  if (!env->ExceptionCheck())
    return false;
  env->ExceptionClear();
  return true;
}
class AndroidHaptics final : public HapticBackend {
public:
  AndroidHaptics() {
    auto* env = environment();
    if (!env)
      return;
    const auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (failed(env) || !activity)
      return;
    const auto type = env->GetObjectClass(activity);
    if (!failed(env) && type) {
      const auto available = env->GetMethodID(type, "hapticsAvailable", "()Z");
      if (available && !failed(env))
        supported_ = env->CallBooleanMethod(activity, available) == JNI_TRUE;
      if (failed(env))
        supported_ = false;
      policy_ = env->GetMethodID(type, "hapticsSetActive", "(Z)V");
      if (failed(env)) policy_ = nullptr;
      play_ = env->GetMethodID(type, "hapticsPerform", "(I)V");
      if (failed(env)) play_ = nullptr;
      activity_ = env->NewGlobalRef(activity);
      if (failed(env)) activity_ = nullptr;
    }
    if (type) env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
  }
  ~AndroidHaptics() override {
    if (auto* env = environment(); env && activity_)
      env->DeleteGlobalRef(activity_);
  }
  bool available() const noexcept override { return supported_ && activity_ && policy_ && play_; }
  void set_active(const bool active) noexcept override {
    if (auto* env = environment(); env && available()) {
      env->CallVoidMethod(activity_, policy_, active ? JNI_TRUE : JNI_FALSE);
      if (failed(env)) supported_ = false;
    }
  }
  void play(const feedback::HapticCue cue) noexcept override {
    if (auto* env = environment(); env && available()) {
      env->CallVoidMethod(activity_, play_, static_cast<jint>(cue));
      if (failed(env)) supported_ = false;
    }
  }

private:
  jobject activity_{};
  jmethodID policy_{}, play_{};
  bool supported_{};
};
} // namespace
std::unique_ptr<HapticBackend> create_haptic_backend() { return std::make_unique<AndroidHaptics>(); }
} // namespace human_resources::platform::detail
