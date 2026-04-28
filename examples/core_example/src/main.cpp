#include "App.h"

#include <cstdio>

int main()
{
  ::std::printf("=== Gecko Core feature tour ===\n\n");
  ::gecko::examples::core_example::App app;
  return app.Run();
}
