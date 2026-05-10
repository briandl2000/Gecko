#include "App.h"

#include <cstdio>

int main()
{
  ::gecko::examples::platform_example::App app;
  const int rc = app.Run();
  ::std::printf(rc == 0 ? "Application exited successfully\n" : "Application exited with code %d\n", rc);
  return rc;
}
