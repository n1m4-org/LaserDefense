#ifndef ENEMY_HPP_
#define ENEMY_HPP_

#include "GameObject/GameObject.hpp"

class Enemy final : public GameObject {
public:
    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
};

#endif // ENEMY_HPP_
