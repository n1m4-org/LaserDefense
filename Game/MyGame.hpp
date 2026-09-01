#ifndef MyGame_HPP_
#define MyGame_HPP_
#include "IGame.hpp"

class MyGame final : public IGame{
public:
    void Initialize(GameEngine::Config& _config) override;

private:
    void Register();
}; // class MyGame

#endif // MyGame_HPP_
