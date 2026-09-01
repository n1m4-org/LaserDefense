#ifndef ENEMY_TEST_SCENE_HPP_
#define ENEMY_TEST_SCENE_HPP_

#include <memory>

#include "Enemy/EnemyManager.hpp"
#include "IScene.hpp"
#include "Model.hpp"

class EnemyTestScene final : public IScene {
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<Model> floor_;

public:
    ~EnemyTestScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
};

#endif // ENEMY_TEST_SCENE_HPP_
