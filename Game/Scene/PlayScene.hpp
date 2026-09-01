#ifndef PLAY_SCENE_HPP_
#define PLAY_SCENE_HPP_

#include <memory>

#include "Enemy/EnemyManager.hpp"
#include "IScene.hpp"
#include "Model.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#include "Tower/TowerManager.hpp"

class Player;
class Laser;

class PlayScene final : public IScene {
    GameSceneInput input_{};
    std::unique_ptr<Player> player_{nullptr};
    std::unique_ptr<Laser> laser_{nullptr};
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<TowerManager> towerManager_;
    std::unique_ptr<Model> floor_;

public:
    PlayScene();
    ~PlayScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
};

#endif // PLAY_SCENE_HPP_
