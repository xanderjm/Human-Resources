#include <human_resources/platform/native_accessibility.h>

#include <utility>

namespace human_resources::platform {
namespace {
class UnavailableAccessibility final : public NativeAccessibility {
public:
  explicit UnavailableAccessibility(std::shared_ptr<accessibility::Session> session)
      : session_(std::move(session)) {}
  ~UnavailableAccessibility() override {
    session_->close();
  }
  [[nodiscard]] accessibility::Preferences preferences() const override {
    return {};
  }
  void update(SDL_Window*, const WindowMetrics&) override {}

private:
  std::shared_ptr<accessibility::Session> session_;
};
} // namespace

std::unique_ptr<NativeAccessibility> create_native_accessibility(std::shared_ptr<accessibility::Session> session) {
  return std::make_unique<UnavailableAccessibility>(std::move(session));
}
} // namespace human_resources::platform
