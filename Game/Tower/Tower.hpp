#ifndef TOWER_HPP_
#define TOWER_HPP_

#include "GameObject/GameObject.hpp"

class Tower final : public GameObject {
public:
    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
};

#endif // TOWER_HPP_
