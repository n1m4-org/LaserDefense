#ifndef ENEMY_MANAGER_HPP_
#define ENEMY_MANAGER_HPP_

#include <memory>
#include <vector>

#include "Enemy.hpp"

class EnemyManager final {
    std::vector<std::unique_ptr<Enemy>> enemies_;

public:
    ~EnemyManager();

    void Initialize();
    void Update(float _deltaTime);
    void Draw() const;
};

#endif // ENEMY_MANAGER_HPP_
