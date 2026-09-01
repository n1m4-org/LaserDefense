#include <memory>
#include "MyGame.hpp"
#include "Framework.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){
    std::unique_ptr<Framework> project = std::make_unique<Framework>();
    project->Execute(std::make_unique<MyGame>());

    return 0;
}

