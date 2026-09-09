#ifndef GIMMICK_MANAGER_HPP_
#define GIMMICK_MANAGER_HPP_

#include <memory>
#include <optional>
#include <string>

#include "Gimmick/IGimmick.hpp"
#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

class GimmickManager final {
    GimmickContext context_{};
    std::unique_ptr<IGimmick> activeGimmick_;
    float spawnInterval_ = 30.0f;
    float spawnTime_ = 0.0f;
    float routeWeight_ = 1.0f;
    float towerDefenseWeight_ = 1.0f;
    float towerOrbitWeight_ = 1.0f;
    float timeLimitSeconds_ = 10.0f;
    float remainingTimeSeconds_ = 0.0f;

    Vector2 timerGaugePosition_{380.0f, 168.0f};
    Vector2 timerGaugeSize_{520.0f, 18.0f};
    float timerGaugeFrameThickness_ = 3.0f;
    Vector4 timerGaugeFrameColor_{0.04f, 0.04f, 0.07f, 0.85f};
    Vector4 timerGaugeColor_{1.0f, 0.2f, 0.2f, 1.0f};
    Sprite timerGaugeFrame_{};
    Sprite timerGaugeFill_{};
    std::string timerLabel_{"GimmickRemainingTime"};
    Vector2 timerLabelPosition_{380.0f, 126.0f};
    float timerLabelFontSize_ = 22.0f;
    Vector4 timerLabelColor_{0.75f, 0.8f, 0.88f, 1.0f};
    Text timerLabelText_{};
    Text timerValueText_{};
    float timerValueRightX_ = 900.0f;
    float timerValuePositionY_ = 124.0f;
    float timerValueFontSize_ = 24.0f;
    float timerValueCharWidthRatio_ = 0.53f;
    bool visible_ = true;
    float opacity_ = 1.0f;
    bool debugPaused_ = false;
    std::optional<GimmickType> pendingStart_;
    std::optional<GimmickType> lastRandomGimmick_;

public:
    void Initialize(const GimmickContext& _context);
    void Update(float _deltaTime);
    void Draw();
    void Debug();
    void SetVisible(bool _visible);
    void SetOpacity(float _opacity);

    const IGimmick* GetActiveGimmick() const { return activeGimmick_.get(); }
    float GetRemainingTimeSeconds() const { return remainingTimeSeconds_; }
    bool GetIndicatorPosition(Vector3& _position) const;

private:
    void LoadConfig();
    void StartRandomGimmick();
    void StartGimmick(GimmickType _type);
    std::unique_ptr<IGimmick> CreateGimmick(GimmickType _type) const;
    void InitializeTimerGauge();
    void UpdateTimerGauge();
};

#endif // GIMMICK_MANAGER_HPP_
