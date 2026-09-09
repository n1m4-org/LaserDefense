#ifndef TOWER_DEFENSE_GIMMICK_HPP_
#define TOWER_DEFENSE_GIMMICK_HPP_

#include <cstdint>
#include <memory>
#include <vector>

#include "Gimmick/IGimmick.hpp"
#include "Math/Vector2.hpp"
#include "Model.hpp"
#include "Sprite.hpp"

class MainTower;

/// @brief メインタワー以外のタワー1つを敵スポナー化し、指定数を倒すまで敵が湧き続けるギミック
/// @note 指定時間内に必要撃破数へ届かなければ失敗になる
class TowerDefenseGimmick final : public IGimmick {
    enum class Phase {
        Warning,     //!< 赤矢印で対象タワーを案内している間。まだ敵は湧かない
        Spawning,    //!< 敵が湧き、制限時間と撃破数を判定している間
        Completion   //!< ノルマ達成後、クリア演出を再生している間
    };

    struct AbsorptionCubeVisual {
        std::unique_ptr<Model> model;
        Vector3 startPosition{};
        float elapsedSeconds = 0.0f;
    };

    struct FragmentVisual {
        std::unique_ptr<Model> model;
        Vector3 position{};
        Vector3 velocity{};
        Vector3 rotation{};
        Vector3 angularVelocity{};
    };

    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;
    MainTower* targetTower_ = nullptr;
    Phase phase_ = Phase::Warning;

    float warningElapsedSeconds_ = 0.0f;
    float warningDurationSeconds_ = 1.0f;
    std::unique_ptr<Sprite> warningArrow_;
    Vector2 warningArrowSize_{32.0f, 32.0f};
    bool warningArrowVisible_ = false;
    std::unique_ptr<Model> spawnerEnemyModel_;
    float spawnerEnemyScaleMultiplier_ = 4.0f;
    float spawnerEnemyStartHeight_ = -0.5f;
    float spawnerEnemyFloatHeight_ = 5.0f;
    float spawnerEnemyRotationSpeed_ = 0.6f;
    float spawnerEnemyRotation_ = 0.0f;
    Vector3 spawnerEnemyBaseScale_{2.0f, 2.0f, 2.0f};
    Vector4 spawnerEnemyBaseColor_{1.0f, 0.0f, 0.0f, 1.0f};
    std::vector<AbsorptionCubeVisual> absorptionCubes_;
    float absorptionSeconds_ = 0.45f;
    float absorptionArcHeight_ = 2.0f;
    float absorptionCubeScaleRatio_ = 0.125f;
    float absorptionPulseSeconds_ = 0.25f;
    float absorptionPulseElapsed_ = 0.0f;
    bool absorptionPulseActive_ = false;
    std::vector<FragmentVisual> fragments_;
    float fragmentScaleRatio_ = 0.5f;
    float fragmentGravity_ = 9.8f;

    float timeLimitSeconds_ = 25.0f;
    int32_t requiredKillCount_ = 10;
    int32_t killCount_ = 0;
    float killRadius_ = 20.0f;
    float spawnRadiusRatio_ = 0.8f;
    float spawnIntervalSeconds_ = 1.5f;
    int32_t spawnCount_ = 3;
    float spawnElapsedSeconds_ = 0.0f;

    std::unique_ptr<Model> baseAoE_;
    std::unique_ptr<Model> progressAoE_;
    Vector4 effectColor_{1.0f, 0.15f, 0.15f, 1.0f};
    float completionSeconds_ = 0.6f;
    float completionElapsed_ = 0.0f;

public:
    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::TowerDefense; }
    GimmickState GetState() const override { return state_; }
    float GetTimeLimitSeconds() const override { return warningDurationSeconds_ + timeLimitSeconds_; }
    void OnTimeLimitExpired() override { Finish(GimmickState::Failed); }

    void Debug() override;

private:
    void LoadConfig();
    void SaveConfig() const;
    void UpdateWarningArrow();
    void InitializeSpawnerEnemyVisual();
    void UpdateSpawnerEnemyVisual(float _deltaTime);
    void SpawnAbsorptionCube(const Vector3& _position);
    void UpdateAbsorptionCubes(float _deltaTime);
    void UpdateAbsorptionPulse(float _deltaTime);
    void InitializeCompletionFragments();
    void UpdateCompletionFragments(float _deltaTime);
    void CollectKillsInRange();
    void InitializeAoEPlane();
    void InitializeCompletionParticles();
    void UpdateProgressVisual();
    void BeginCompletion();
    void UpdateCompletion(float _deltaTime);
    void Finish(GimmickState _result);
};

#endif // TOWER_DEFENSE_GIMMICK_HPP_
