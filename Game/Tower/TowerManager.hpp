#ifndef TOWER_MANAGER_HPP_
#define TOWER_MANAGER_HPP_

#include <memory>
#include <vector>

#include "Tower.hpp"

class MainTower;

class TowerManager final {
    std::vector<std::unique_ptr<Tower>> towers_;

public:
    void Initialize();
    Tower* AddTower(const Vector3& _position);
    MainTower* AddMainTower(const Vector3& _position);
    void Update(float _deltaTime);
    void Draw() const;
    Tower* PickTower(const Vector3& _origin, const Vector3& _direction, float _length) const;
    void SetHoveredTower(const Tower* _tower);
};

#endif // TOWER_MANAGER_HPP_
