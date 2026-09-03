#ifndef MAIN_TOWER_HPP_
#define MAIN_TOWER_HPP_

#include "Tower.hpp"

class MainTower final : public Tower {
    std::unique_ptr<Model> baseModel_;
    std::unique_ptr<Collision::Collider> baseCollider_;

public:
    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
    void SetHovered(bool _hovered) override;
};

#endif // MAIN_TOWER_HPP_
