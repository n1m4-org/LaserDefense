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
#include "Sprite.hpp"
#include "SurvivalTime/SurvivalTimeManager.hpp"
#include "Text/Text.hpp"
#include "Tower/TowerHpGauge.hpp"
#include "Tower/TowerManager.hpp"
#include "UI/MainTowerIndicator.hpp"
#include "src/ParticleSystem/Emitter/Emitter.hpp"

class MainTower;
class Player;
class PlayerCamera;
class Laser;
class Tower;

class PlayScene final : public IScene {
    struct PlayerSpeedParticleState {
        Vector3 direction{1.0f, 0.0f, 0.0f};
    };

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
    /// リザルト。見た目と配置は Assets/Data/UI/Result.json が持ち、UIエディタから編集できる
    std::unique_ptr<ResultOverlay> resultOverlay_;
    std::unique_ptr<MainTowerIndicator> mainTowerIndicator_;
    std::unique_ptr<MainTowerIndicator> gimmickIndicator_;
    /// HP が尽きたらリザルトを出す対象。所有者は towerManager_
    MainTower* mainTower_ = nullptr;
    std::unique_ptr<Model> floor_;
    std::unique_ptr<Model> shockwave_;
    float shockwaveTime_ = 0.5f;
    static constexpr float SHOCKWAVE_DURATION = 0.5f;
    static constexpr float SHOCKWAVE_RADIUS = 20.0f;
    static constexpr float SHOCKWAVE_SPEED = 35.0f;
    std::array<std::unique_ptr<Model>, 8> fences_;
    float stageSize_ = 200.0f;
    float towerMargin_ = 20.0f;
    float fenceHeight_ = 2.0f;
    float wallBounce_ = 0.8f;
    std::array<Sprite, 4> reticleOutlines_{};
    std::array<Sprite, 4> reticleFills_{};
    Vector2 reticlePosition_{};
    bool reticlePositionInitialized_ = false;
    float reticleHoverProgress_ = 0.0f;
    bool cursorVisible_ = false;
    Text clickTowerGuide_{};
    float clickTowerGuideElapsed_ = 0.0f;
    bool clickTowerGuideVisible_ = false;
    Text dashInputGuide_{};
    Text dashActionGuide_{};
    Sprite dashCooldownGaugeFrame_{};
    Sprite dashCooldownGauge_{};
    float dashCooldownRatio_ = 1.0f;
    float dashCooldownFlashTime_ = 0.0f;
    float hudFadeElapsed_ = 0.0f;
    float hudFadeDuration_ = 0.45f;
    float hudOpacity_ = 0.0f;
    std::shared_ptr<PlayerSpeedParticleState> playerSpeedParticleState_;
    EmitterHandle playerSpeedEffectHandle_;
    static constexpr float CLICK_TOWER_GUIDE_DURATION = 10.0f;
    static constexpr float RETICLE_FOLLOW_SPEED = 18.0f;
    static constexpr float RETICLE_HOVER_DURATION = 0.1f;
    static constexpr float DASH_COOLDOWN_FLASH_DURATION = 0.2f;
    Tower* assistedTower_ = nullptr;

public:
    PlayScene();
    ~PlayScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void Debug() override;

private:
    void LoadStageConfig();
    void UpdateTowerSelection(float _deltaTime);
    void InitializePlayerSpeedEffect();
    void InitializePlayerDashEffect();
    void UpdatePlayerSpeedEffect(float _speed, float _maxSpeed, float _deltaTime);
    void EmitPlayerDashEffect();
    void ApplyDashCooldownGauge();

    /// @brief タイトルシーンへ戻る
    /// @note UI のボタンからもキー入力からも呼ばれる。二重に呼んでも SceneSwitcher 側で弾かれる
    void RequestReturnToTitle();

    /// @brief 画面に常駐する UI を描画キューへ積む
    /// @note リザルトの暗幕は Canvas 側(= これより後)で描かれるので、ここで積んだ UI ごと沈む
    void DrawHud();
};

#endif // PLAY_SCENE_HPP_
