#include "core/application.h"
#include "core/startup_timing.h"
#include "path.h"

int main(int argc, char **argv) {
  Rodan::StartupTiming::Begin();
  Rodan::Application app;
  Velos::Path::Initialize(argv[0]);
  Rodan::StartupTiming::MarkCheckpoint("Application + path setup");
  app.Run();
  return 0;
}
