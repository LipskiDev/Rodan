#include "core/application.h"

#include <iostream>

namespace Rodan {

Application::Application() { std::cout << "[Rodan] Application created\n"; }

Application::~Application() { std::cout << "[Rodan] Application destroyed\n"; }

void Application::Run() { std::cout << "[Rodan] Run\n"; }

} // namespace Rodan
