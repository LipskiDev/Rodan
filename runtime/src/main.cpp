#include "core/application.h"
#include "path.h"

int main(int argc, char **argv) {
  Rodan::Application app;
  Velos::Path::Initialize(argv[0]);
  app.Run();
  return 0;
}
