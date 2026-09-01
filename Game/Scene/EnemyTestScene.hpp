#ifndef ENEMY_TEST_SCENE_HPP_
#define ENEMY_TEST_SCENE_HPP_

#include <memory>

#include "Enemy/EnemyManager.hpp"
#include "IScene.hpp"

class EnemyTestScene final : public IScene {
    std::unique_ptr<EnemyManager> enemyManager_;

public:
    ~EnemyTestScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
};

#endif // ENEMY_TEST_SCENE_HPP_
