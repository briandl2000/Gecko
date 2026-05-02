/// @file
/// Default `main()` for benchmark binaries. Linked from
/// `Gecko::BenchMain`; benches that need pre-`main` setup can omit
/// linking this and provide their own main calling
/// `gecko::bench::Main(argc, argv)`.

#include <gecko/bench/bench.h>

int main(int argc, char** argv)
{
  return ::gecko::bench::Main(argc, argv);
}
