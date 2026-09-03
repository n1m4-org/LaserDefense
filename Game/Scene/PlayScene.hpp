#ifndef PLAY_SCENE_HPP_
#define PLAY_SCENE_HPP_

#include <memory>

#include "Enemy/EnemyManager.hpp"
#include "IScene.hpp"
#include "Model.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#include "Score/ScoreManager.hpp"
#include "Tower/TowerManager.hpp"

class Player;
class PlayerCamera;
class Laser;
class Line;

class PlayScene final : public IScene {
    GameSceneInput input_{};
    std::unique_ptr<Player> player_{nullptr};
    std::unique_ptr<PlayerCamera> playerCamera_;
    std::unique_ptr<Laser> laser_{nullptr};
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<TowerManager> towerManager_;
    std::unique_ptr<ScoreManager> scoreManager_;
    std::unique_ptr<Model> floor_;
    std::unique_ptr<Line> mouseCursor_;
    bool cursorVisible_ = false;

public:
    PlayScene();
    ~PlayScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;

private:
    void UpdateTowerSelection();
};

#endif // PLAY_SCENE_HPP_
