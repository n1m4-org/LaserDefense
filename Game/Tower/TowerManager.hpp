#ifndef TOWER_MANAGER_HPP_
#define TOWER_MANAGER_HPP_

#include <memory>
#include <vector>

#include "Tower.hpp"

class TowerManager final {
    std::vector<std::unique_ptr<Tower>> towers_;

public:
    void Initialize();
    void AddTower(const Vector3& _position);
    void Update(float _deltaTime);
    void Draw() const;
};

#endif // TOWER_MANAGER_HPP_
