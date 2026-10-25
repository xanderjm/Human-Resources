#include <human_resources/platform/native_accessibility.h>

#import <AppKit/AppKit.h>

#include <memory>
#include <string>

namespace hr = human_resources;

@interface HumanResourcesMacAccessibilityElement : NSAccessibilityElement {
@public
  std::weak_ptr<hr::accessibility::Session> session;
  std::string identity;
}
@end

@implementation HumanResourcesMacAccessibilityElement
- (BOOL)accessibilityPerformPress {
  const auto owner = session.lock();
  return owner && owner->request(identity, hr::accessibility::ActionKind::activate);
}
- (BOOL)accessibilityPerformIncrement {
  const auto owner = session.lock();
  return owner && owner->request(identity, hr::accessibility::ActionKind::increment);
}
- (BOOL)accessibilityPerformDecrement {
  const auto owner = session.lock();
  return owner && owner->request(identity, hr::accessibility::ActionKind::decrement);
}
@end

@interface HumanResourcesMacAccessibilitySurface : NSObject
@property(nonatomic, weak) NSView* container;
@property(nonatomic, strong) NSDictionary<NSString*, HumanResourcesMacAccessibilityElement*>* retained;
@property(nonatomic, strong) NSArray* elements;
@property(nonatomic, strong) NSArray* previousChildren;
@property(nonatomic, copy) NSString* previousRole;
@property(nonatomic, copy) NSString* previousLabel;
@property(nonatomic) BOOL previousElement;
- (void)detach;
@end

@implementation HumanResourcesMacAccessibilitySurface
- (void)detach {
  if ([self.container.accessibilityChildren isEqualToArray:self.elements]) {
    self.container.accessibilityChildren = self.previousChildren;
    self.container.accessibilityRole = self.previousRole;
    self.container.accessibilityLabel = self.previousLabel;
    self.container.accessibilityElement = self.previousElement;
  }
  self.container = nil;
  self.retained = nil;
  self.elements = nil;
  self.previousChildren = nil;
}
@end

namespace {
NSString* native_string(const std::string& value) {
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
}

NSAccessibilityRole role(const hr::ui::AccessibilityNode& node) {
  switch (node.role) {
  case hr::ui::SemanticRole::button: return NSAccessibilityButtonRole;
  case hr::ui::SemanticRole::toggle: return NSAccessibilityCheckBoxRole;
  case hr::ui::SemanticRole::slider: return NSAccessibilitySliderRole;
  case hr::ui::SemanticRole::image: return NSAccessibilityImageRole;
  case hr::ui::SemanticRole::label: return NSAccessibilityStaticTextRole;
  default: return NSAccessibilityGroupRole;
  }
}

void apply_snapshot(HumanResourcesMacAccessibilitySurface* surface, NSWindow* window,
    const std::vector<hr::ui::AccessibilityNode>& nodes,
    const std::shared_ptr<hr::accessibility::Session>& session) {
  NSView* container = window.contentView;
  if (container == nil) return;
  if (surface.container != container) {
    [surface detach];
    surface.container = container;
    surface.previousChildren = container.accessibilityChildren;
    surface.previousRole = container.accessibilityRole;
    surface.previousLabel = container.accessibilityLabel;
    surface.previousElement = container.isAccessibilityElement;
  }
  container.accessibilityElement = YES;
  container.accessibilityRole = NSAccessibilityGroupRole;
  container.accessibilityLabel = @"Human Resources";
  auto* retained = [NSMutableDictionary<NSString*, HumanResourcesMacAccessibilityElement*> dictionary];
  auto* ordered = [NSMutableArray array];
  for (const auto& node : nodes) {
    if (node.label.empty() || node.frame.width <= 0 || node.frame.height <= 0) continue;
    NSString* key = native_string(node.stable_id);
    if (key == nil) continue;
    auto* element = surface.retained[key];
    if (element == nil) element = [[HumanResourcesMacAccessibilityElement alloc] init];
    element->identity = node.stable_id;
    element->session = session;
    element.accessibilityElement = YES;
    element.accessibilityParent = container;
    element.accessibilityIdentifier = key;
    element.accessibilityRole = role(node);
    element.accessibilityLabel = native_string(node.label);
    element.accessibilityTitle = native_string(node.label);
    element.accessibilityEnabled = !node.disabled;
    element.accessibilitySelected = node.selected;
    element.accessibilityValue = node.role == hr::ui::SemanticRole::toggle ? @(node.checked) : native_string(node.value);
    element.accessibilityHelp = native_string(node.value);
    const auto& r = node.frame;
    const CGFloat y = container.isFlipped ? NSMinY(container.bounds) + r.y : NSMaxY(container.bounds) - r.y - r.height;
    element.accessibilityFrameInParentSpace = NSMakeRect(NSMinX(container.bounds) + r.x, y, r.width, r.height);
    retained[key] = element;
    [ordered addObject:element];
  }
  const BOOL changed = ![surface.elements isEqualToArray:ordered];
  surface.retained = retained;
  surface.elements = [ordered copy];
  container.accessibilityChildren = surface.elements;
  if (changed) NSAccessibilityPostNotification(container, NSAccessibilityLayoutChangedNotification);
}

class MacAccessibility final : public hr::platform::NativeAccessibility {
public:
  explicit MacAccessibility(std::shared_ptr<hr::accessibility::Session> session)
      : session_(std::move(session)), surface_([[HumanResourcesMacAccessibilitySurface alloc] init]) {}
  ~MacAccessibility() override {
    session_->close();
    auto* surface = surface_;
    if ([NSThread isMainThread]) [surface detach];
    else dispatch_async(dispatch_get_main_queue(), ^{ [surface detach]; });
  }
  hr::accessibility::Preferences preferences() const override {
    __block hr::accessibility::Preferences result;
    void (^read)(void) = ^{
      result = {.text_scale = 1.F,
          .reduced_motion = NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion,
          .native_bridge_available = true};
    };
    if ([NSThread isMainThread]) read();
    else dispatch_sync(dispatch_get_main_queue(), read);
    return result;
  }
  void update(SDL_Window* window, const hr::platform::WindowMetrics& metrics) override {
    if (window == nullptr || !metrics.valid) return;
    void* pointer = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (pointer == nullptr) return;
    NSWindow* native_window = (__bridge NSWindow*)pointer;
    auto nodes = session_->snapshot();
    for (auto& node : nodes) node.frame = hr::platform::window_coordinates(node.frame, metrics);
    auto session = session_;
    auto* surface = surface_;
    if ([NSThread isMainThread]) apply_snapshot(surface, native_window, nodes, session);
    else dispatch_async(dispatch_get_main_queue(), ^{ apply_snapshot(surface, native_window, nodes, session); });
  }
private:
  std::shared_ptr<hr::accessibility::Session> session_;
  __strong HumanResourcesMacAccessibilitySurface* surface_;
};
} // namespace

namespace human_resources::platform {
std::unique_ptr<NativeAccessibility> create_native_accessibility(std::shared_ptr<accessibility::Session> session) {
  return std::make_unique<MacAccessibility>(std::move(session));
}
} // namespace human_resources::platform
