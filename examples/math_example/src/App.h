#pragma once

namespace gecko::examples::math_example {

/// Walks through the public surface of the `gecko::math` module.
///
/// Each `Run*` method is a self-contained demo of one topic. They are
/// grouped here so a newcomer can read them top-to-bottom and learn the
/// API by example. The class itself is stateless -- it exists purely to
/// give the demos a shared home and a single entry point (`Run()`).
class App
{
public:
  /// Runs every demo in order and prints results to stdout.
  /// Returns the process exit code (always 0 today).
  int Run();

private:
  void RunVectorBasics();
  void RunMatrixTransforms();
  void RunCameraMatrices();
  void RunAabbAndRect();
  void RunQuaternions();
  void RunUtilities();
};

}  // namespace gecko::examples::math_example
