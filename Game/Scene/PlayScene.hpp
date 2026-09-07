#ifndef PLAY_SCENE_HPP_
#define PLAY_SCENE_HPP_

#include <memory>
#include <array>

#include "Combo/ComboManager.hpp"
#include "Enemy/EnemyManager.hpp"
#include "Gimmick/GimmickManager.hpp"
#include "IScene.hpp"
#include "Model.hpp"
#include "Result/ResultOverlay.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#include "Score/ScoreManager.hpp"
#include "SurvivalTime/SurvivalTimeManager.hpp"
#include "Tower/TowerHpGauge.hpp"
#include "Tower/TowerManager.hpp"

class MainTower;
class Player;
class PlayerCamera;
class Laser;
class Line;
class Tower;

class PlayScene final : public IScene {
    GameSceneInput input_{};
    std::unique_ptr<Player> player_{nullptr};
    std::unique_ptr<PlayerCamera> playerCamera_;
    std::unique_ptr<Laser> laser_{nullptr};
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<GimmickManager> gimmickManager_;
    std::unique_ptr<TowerManager> towerManager_;
    std::unique_ptr<ScoreManager> scoreManager_;
    std::unique_ptr<SurvivalTimeManager> survivalTimeManager_;
    std::unique_ptr<ComboManager> comboManager_;
    std::unique_ptr<TowerHpGauge> towerHpGauge_;
    std::unique_ptr<ResultOverlay> resultOverlay_;
    /// HP が尽きたらリザルトを出す対象。所有者は towerManager_
    MainTower* mainTower_ = nullptr;
    std::unique_ptr<Model> floor_;
    std::array<std::unique_ptr<Model>, 8> fences_;
    float stageSize_ = 200.0f;
    float towerMargin_ = 20.0f;
    float fenceHeight_ = 2.0f;
    float wallBounce_ = 0.8f;
    std::unique_ptr<Line> mouseCursor_;
    bool cursorVisible_ = false;
    Tower* assistedTower_ = nullptr;

public:
    PlayScene();
    ~PlayScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;

private:
    void LoadStageConfig();
    void UpdateTowerSelection();

    /// @brief 画面に常駐する UI とリザルトを描画キューへ積む
    /// @note リザルトの暗幕がゲーム中の UI も覆えるように、最後にまとめて呼ぶ
    void DrawHud();
};

#endif // PLAY_SCENE_HPP_
