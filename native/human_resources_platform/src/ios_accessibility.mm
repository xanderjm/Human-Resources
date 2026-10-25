#include <human_resources/platform/native_accessibility.h>

#import <UIKit/UIKit.h>

#include <cmath>
#include <memory>
#include <string>

namespace nf = human_resources;

@interface HumanResourcesAccessibilityElement : UIAccessibilityElement {
@public
  std::weak_ptr<nf::accessibility::Session> session;
  std::string identity;
}
@end

@implementation HumanResourcesAccessibilityElement
- (BOOL)accessibilityActivate {
  const auto owner = session.lock();
  return owner && owner->request(identity, nf::accessibility::ActionKind::activate);
}
- (void)accessibilityIncrement {
  if (const auto owner = session.lock())
    (void)owner->request(identity, nf::accessibility::ActionKind::increment);
}
- (void)accessibilityDecrement {
  if (const auto owner = session.lock())
    (void)owner->request(identity, nf::accessibility::ActionKind::decrement);
}
@end

@interface HumanResourcesAccessibilitySurface : NSObject
@property(nonatomic, weak) UIView* container;
@property(nonatomic, strong) NSMutableDictionary<NSString*, HumanResourcesAccessibilityElement*>* retained;
@property(nonatomic, strong) NSArray* elements;
- (void)detach;
@end

@implementation HumanResourcesAccessibilitySurface
- (void)detach {
  if (self.container.accessibilityElements == self.elements)
    self.container.accessibilityElements = nil;
  self.container = nil;
  self.retained = nil;
  self.elements = nil;
}
@end

namespace {
NSString* native_string(const std::string& value) {
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
}

UIAccessibilityTraits traits(const nf::ui::AccessibilityNode& node) {
  UIAccessibilityTraits result = UIAccessibilityTraitNone;
  switch (node.role) {
  case nf::ui::SemanticRole::button:
  case nf::ui::SemanticRole::toggle:
    result |= UIAccessibilityTraitButton;
    break;
  case nf::ui::SemanticRole::image:
    result |= UIAccessibilityTraitImage;
    break;
  case nf::ui::SemanticRole::slider:
    result |= UIAccessibilityTraitAdjustable;
    break;
  case nf::ui::SemanticRole::label:
    result |= UIAccessibilityTraitStaticText;
    break;
  default:
    break;
  }
  if (node.disabled)
    result |= UIAccessibilityTraitNotEnabled;
  if (node.checked || node.selected)
    result |= UIAccessibilityTraitSelected;
  return result;
}

void apply_snapshot(HumanResourcesAccessibilitySurface* surface, UIWindow* window,
    const std::vector<nf::ui::AccessibilityNode>& nodes,
    const std::shared_ptr<nf::accessibility::Session>& session) {
  UIView* container = window.rootViewController.view;
  if (container == nil)
    return;
  if (surface.container != container) {
    [surface detach];
    surface.container = container;
    surface.retained = [NSMutableDictionary dictionary];
  }
  container.isAccessibilityElement = NO;
  auto* retained = [NSMutableDictionary<NSString*, HumanResourcesAccessibilityElement*> dictionary];
  auto* ordered = [NSMutableArray array];
  for (const auto& node : nodes) {
    if (node.frame.width <= 0.0F || node.frame.height <= 0.0F || node.label.empty())
      continue;
    NSString* key = native_string(node.stable_id);
    if (key == nil)
      continue;
    auto* element = surface.retained[key];
    if (element == nil)
      element = [[HumanResourcesAccessibilityElement alloc] initWithAccessibilityContainer:container];
    element->identity = node.stable_id;
    element->session = session;
    element.accessibilityLabel = native_string(node.label);
    element.accessibilityValue = native_string(node.value);
    element.accessibilityTraits = traits(node);
    const auto frame = CGRectMake(node.frame.x, node.frame.y, node.frame.width, node.frame.height);
    element.accessibilityFrameInContainerSpace = [container convertRect:frame fromView:window];
    retained[key] = element;
    [ordered addObject:element];
  }
  // Retaining the same elements preserves VoiceOver focus across re-layouts.
  // Do not post a focus-changing notification for every value update.
  surface.retained = retained;
  surface.elements = ordered;
  container.accessibilityElements = ordered;
}

class IosAccessibility final : public nf::platform::NativeAccessibility {
public:
  explicit IosAccessibility(std::shared_ptr<nf::accessibility::Session> session)
      : session_(std::move(session)), surface_([[HumanResourcesAccessibilitySurface alloc] init]) {}
  ~IosAccessibility() override {
    session_->close();
    auto* surface = surface_;
    if ([NSThread isMainThread])
      [surface detach];
    else
      dispatch_async(dispatch_get_main_queue(), ^{
        [surface detach];
      });
  }
  nf::accessibility::Preferences preferences() const override {
    __block nf::accessibility::Preferences result;
    void (^read)(void) = ^{
      const float scale = static_cast<float>([[UIFontMetrics defaultMetrics] scaledValueForValue:16.0] / 16.0);
      result = {.text_scale = std::isfinite(scale) && scale > 0.0F ? scale : 1.0F,
          .reduced_motion = UIAccessibilityIsReduceMotionEnabled(),
          .native_bridge_available = true};
    };
    if ([NSThread isMainThread])
      read();
    else
      dispatch_sync(dispatch_get_main_queue(), read);
    return result;
  }
  void update(SDL_Window* window, const nf::platform::WindowMetrics& metrics) override {
    if (window == nullptr || !metrics.valid)
      return;
    void* pointer = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    if (pointer == nullptr)
      return;
    UIWindow* native_window = (__bridge UIWindow*)pointer;
    auto nodes = session_->snapshot();
    for (auto& node : nodes)
      node.frame = nf::platform::window_coordinates(node.frame, metrics);
    auto session = session_;
    auto* surface = surface_;
    if ([NSThread isMainThread])
      apply_snapshot(surface, native_window, nodes, session);
    else
      dispatch_async(dispatch_get_main_queue(), ^{
        apply_snapshot(surface, native_window, nodes, session);
      });
  }

private:
  std::shared_ptr<nf::accessibility::Session> session_;
  __strong HumanResourcesAccessibilitySurface* surface_;
};
} // namespace

namespace human_resources::platform {
std::unique_ptr<NativeAccessibility> create_native_accessibility(std::shared_ptr<accessibility::Session> session) {
  return std::make_unique<IosAccessibility>(std::move(session));
}
} // namespace human_resources::platform
