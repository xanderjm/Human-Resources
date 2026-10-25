#include <human_resources/platform/native_accessibility.h>

#include <SDL3/SDL_system.h>
#include <jni.h>

#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
#include <utility>

namespace human_resources::platform {
namespace {
std::mutex registry_mutex;
std::map<jlong, std::weak_ptr<accessibility::Session>> sessions;
std::atomic<jlong> next_handle{1};

class Activity final {
public:
  Activity() : env(static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv())) {
    if (env != nullptr) {
      object = static_cast<jobject>(SDL_GetAndroidActivity());
      if (object != nullptr)
        type = env->GetObjectClass(object);
    }
  }
  ~Activity() {
    if (env == nullptr)
      return;
    if (env->ExceptionCheck())
      env->ExceptionClear();
    if (type != nullptr)
      env->DeleteLocalRef(type);
    if (object != nullptr)
      env->DeleteLocalRef(object);
  }
  Activity(const Activity&) = delete;
  Activity& operator=(const Activity&) = delete;
  jmethodID method(const char* name, const char* signature) const {
    if (type == nullptr)
      return nullptr;
    const auto result = env->GetMethodID(type, name, signature);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return nullptr;
    }
    return result;
  }
  JNIEnv* env{};
  jobject object{};
  jclass type{};
};

class AndroidAccessibility final : public NativeAccessibility {
public:
  explicit AndroidAccessibility(std::shared_ptr<accessibility::Session> session)
      : session_(std::move(session)), handle_(next_handle.fetch_add(1)) {
    const std::scoped_lock lock(registry_mutex);
    sessions[handle_] = session_;
  }
  ~AndroidAccessibility() override {
    session_->close();
    {
      const std::scoped_lock lock(registry_mutex);
      sessions.erase(handle_);
    }
    Activity activity;
    if (const auto method = activity.method("accessibilityClose", "(J)V"))
      activity.env->CallVoidMethod(activity.object, method, handle_);
  }
  accessibility::Preferences preferences() const override {
    accessibility::Preferences result;
    Activity activity;
    const auto scale = activity.method("accessibilityTextScale", "()F");
    const auto motion = activity.method("accessibilityReducedMotion", "()Z");
    if (scale == nullptr || motion == nullptr)
      return result;
    const float value = activity.env->CallFloatMethod(activity.object, scale);
    if (activity.env->ExceptionCheck())
      return result;
    result.text_scale = std::isfinite(value) && value > 0.0F ? value : 1.0F;
    result.reduced_motion = activity.env->CallBooleanMethod(activity.object, motion) == JNI_TRUE;
    if (activity.env->ExceptionCheck())
      return {};
    result.native_bridge_available = true;
    return result;
  }
  void update(SDL_Window*, const WindowMetrics& metrics) override {
    Activity activity;
    const auto method = activity.method("accessibilityUpdate", "(JLjava/lang/String;)V");
    if (method == nullptr || !metrics.valid)
      return;
    auto nodes = session_->snapshot();
    for (auto& node : nodes)
      node.frame = window_coordinates(node.frame, metrics);
    const auto encoded = accessibility::serialize(nodes);
    const jstring value = activity.env->NewStringUTF(encoded.c_str());
    if (value == nullptr)
      return;
    activity.env->CallVoidMethod(activity.object, method, handle_, value);
    activity.env->DeleteLocalRef(value);
  }

private:
  std::shared_ptr<accessibility::Session> session_;
  jlong handle_;
};

std::string decode_identity(const std::string_view encoded) {
  if (encoded.size() % 2U != 0U)
    return {};
  auto nibble = [](const char value) -> int {
    if (value >= '0' && value <= '9')
      return value - '0';
    if (value >= 'a' && value <= 'f')
      return value - 'a' + 10;
    return -1;
  };
  std::string result;
  for (std::size_t index = 0; index < encoded.size(); index += 2U) {
    const int high = nibble(encoded[index]);
    const int low = nibble(encoded[index + 1U]);
    if (high < 0 || low < 0)
      return {};
    result.push_back(static_cast<char>(high * 16 + low));
  }
  return result;
}
} // namespace

std::unique_ptr<NativeAccessibility> create_native_accessibility(std::shared_ptr<accessibility::Session> session) {
  return std::make_unique<AndroidAccessibility>(std::move(session));
}

bool request_native_action(const jlong handle, const std::string_view identity, const jint action) {
  std::shared_ptr<accessibility::Session> session;
  {
    const std::scoped_lock lock(registry_mutex);
    const auto found = sessions.find(handle);
    if (found != sessions.end())
      session = found->second.lock();
  }
  if (!session || action < 0 || action > 2)
    return false;
  return session->request(decode_identity(identity), static_cast<accessibility::ActionKind>(action));
}
} // namespace human_resources::platform

extern "C" JNIEXPORT jboolean JNICALL
Java_com_xanderjm_humanresources_AccessibilityLayer_nativeAccessibilityAction(
    JNIEnv* env, jclass, const jlong handle, jstring identity, const jint action) {
  if (identity == nullptr)
    return JNI_FALSE;
  const char* encoded = env->GetStringUTFChars(identity, nullptr);
  if (encoded == nullptr)
    return JNI_FALSE;
  const bool accepted = human_resources::platform::request_native_action(handle, encoded, action);
  env->ReleaseStringUTFChars(identity, encoded);
  return accepted ? JNI_TRUE : JNI_FALSE;
}
