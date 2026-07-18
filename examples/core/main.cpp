#include "gecko/gecko.h"

namespace {

constexpr gecko::Label ExampleLabel = gecko::MakeLabel("example.core");

}  // namespace

int main()
{
  gecko::GeckoConfig config {};
  config.AppName = "Gecko Core Example";
  config.Platform.Backend = gecko::platform::DisplayBackendKind::Null;
  config.EnableGraphics = false;

  if (gecko::Initialize(config) != gecko::InitializeResult::Success)
    return 1;

  gecko::i32 answer = 0;
  const gecko::JobHandle job =
      gecko::SubmitJob([&answer]() { answer = 6 * 7; }, gecko::JobPriority::Normal, ExampleLabel);
  gecko::WaitForJob(job);

  const gecko::MemoryStats memory = gecko::GetMemoryStats();
  GECKO_INFO(ExampleLabel, "job answer={}, workers={}, live memory={} bytes", answer, gecko::JobWorkerCount(),
             memory.LiveBytes);

  gecko::Shutdown();
  return answer == 42 ? 0 : 2;
}
