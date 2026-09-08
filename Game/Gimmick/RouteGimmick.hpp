#ifndef ROUTE_GIMMICK_HPP_
#define ROUTE_GIMMICK_HPP_

#include <array>
#include <memory>
#include <vector>

#include "Collision/Collider.h"
#include "Gimmick/IGimmick.hpp"
#include "Math/Vector3.hpp"
#include "Model.hpp"
#include "Text/Text.hpp"

enum class RouteColor : uint8_t { Red, Blue, Green, Yellow };

class RouteGimmick final : public IGimmick {
public:
    enum class Mode { SingleColorTutorial, GuidedTutorial, Normal };

private:
    struct ColorFloor {
        RouteColor color = RouteColor::Red;
        Vector3 position{};
        bool cleared = false;
        float disappearTime = 0.0f;
        std::unique_ptr<Collision::Collider> collider;
        std::unique_ptr<Model> model;
    };

    static inline bool singleColorTutorialCleared_ = false;
    static inline bool guidedTutorialCleared_ = false;

    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;
    Mode mode_ = Mode::Normal;

    std::array<RouteColor, 4> colorOrder_{};
    int32_t colorCount_ = 4;
    int32_t nextFloorIndex_ = 0;
    std::vector<ColorFloor> floors_;
    Vector3 towerPosition_{};

    float elapsedTime_ = 0.0f;
    float timeLimitSeconds_ = 20.0f;
    float floorRadius_ = 1.5f;
    float floorOpacity_ = 0.55f;
    float floorDisappearDuration_ = 0.35f;
    float minFloorDistance_ = 6.0f;
    float minTowerDistance_ = 4.0f;
    float placementRadius_ = 18.0f;

    float colorRevealInterval_ = 0.6f;
    float guidedColorRevealInterval_ = 1.2f;
    float guideNumberLeadSeconds_ = 0.5f;
    int32_t revealedColorCount_ = 0;
    float colorRevealTimer_ = 0.0f;
    bool guideNumberActive_ = false;
    mutable Text guideNumberText_{};
    bool pendingAdvanceToGuidedTutorial_ = false;

public:
    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::Route; }
    GimmickState GetState() const override { return state_; }

    Mode GetMode() const { return mode_; }

    void Debug() override;

    static void ResetTutorialProgress() {
        singleColorTutorialCleared_ = false;
        guidedTutorialCleared_ = false;
    }

private:
    void LoadConfig();
    void SaveConfig() const;
    void DetermineMode();
    void GenerateColorOrder();
    void PlaceFloors();
    Vector3 GenerateFloorPosition() const;
    void UpdateFloors(float _deltaTime);
    void UpdateFloorDisappear(ColorFloor& _floor, float _deltaTime);
    void OnFloorEntered(std::size_t _index);
    void AdvanceToGuidedTutorial();
    void UpdateColorReveal(float _deltaTime);
    void StartColorRevealStep();
    void ShowGuideNumber(int32_t _step);
    void HideGuideNumber();
    void RegisterParticleTemplates() const;
    void EmitColorReveal(RouteColor _color) const;
    void EmitFloorClear(RouteColor _color, const Vector3& _position) const;
    void Finish(GimmickState _result);
};

#endif // ROUTE_GIMMICK_HPP_
