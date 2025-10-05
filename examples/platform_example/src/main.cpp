#include <gecko/platform/platform_context.h>

using namespace gecko;
using namespace gecko::platform;

int main() {
  PlatformConfig cfg = {};

  Unique<PlatformContext> ctx = PlatformContext::Create(cfg);
  return 0;
}
