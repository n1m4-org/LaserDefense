#ifndef TOWER_ORBIT_GIMMICK_HPP_
#define TOWER_ORBIT_GIMMICK_HPP_

#include <array>
#include <memory>

#include "Gimmick/IGimmick.hpp"
#include "Model.hpp"
#include "src/ParticleSystem/Emitter/Emitter.hpp"

class Tower;

/// @brief 指定されたタワーの周囲を旋回するギミック。
class TowerOrbitGimmick final : public IGimmick {
    enum class Phase {
        Orbit,
        Completion
    };

    enum class OrbitDirection {
        Clockwise,
        CounterClockwise
    };

    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;
    Phase phase_ = Phase::Orbit;
    OrbitDirection direction_ = OrbitDirection::Clockwise;
    Tower* targetTower_ = nullptr;

    std::unique_ptr<Model> baseAoE_;
    std::unique_ptr<Model> progressAoE_;
    std::array<std::unique_ptr<Model>, 4> arrows_{};
    EmitterHandle ambientEffect_;
    Vector4 effectColor_{0.15f, 1.0f, 0.35f, 1.0f};

    float requiredDegrees_ = 1080.0f;
    float aoeRadius_ = 20.0f;
    float completionSeconds_ = 0.6f;
    float shockwaveScale_ = 1.8f;
    float arrowRadiusRatio_ = 0.65f;
    float arrowSize_ = 3.0f;
    float arrowIdleSpeedDegrees_ = 15.0f;
    float arrowActiveSpeedDegrees_ = 100.0f;
    float timeLimitSeconds_ = 10.0f;

    float accumulatedAngle_ = 0.0f;
    float previousAngle_ = 0.0f;
    bool hasPreviousAngle_ = false;
    bool isPlayerOrbiting_ = false;
    float arrowRotation_ = 0.0f;
    float completionElapsed_ = 0.0f;

public:
    ~TowerOrbitGimmick() override;

    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::TowerOrbit; }
    GimmickState GetState() const override { return state_; }
    float GetTimeLimitSeconds() const override { return timeLimitSeconds_; }
    void OnTimeLimitExpired() override { state_ = GimmickState::Failed; }

private:
    void LoadConfig();
    void SelectTargetTower();
    void InitializeVisuals();
    void InitializeParticles();
    void UpdateOrbit(float _deltaTime);
    void UpdateArrowVisuals(float _alpha = 1.0f);
    void UpdateCompletion(float _deltaTime);
    void BeginCompletion();
};

#endif // TOWER_ORBIT_GIMMICK_HPP_
