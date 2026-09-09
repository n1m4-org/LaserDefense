#ifndef ROUTE_GIMMICK_HPP_
#define ROUTE_GIMMICK_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Collision/Collider.h"
#include "Gimmick/IGimmick.hpp"
#include "Math/Vector3.hpp"
#include "Model.hpp"
#include "src/ParticleSystem/Emitter/Emitter.hpp"

enum class RouteColor : uint8_t { Red, Blue, Green, Yellow };

class RouteGimmick final : public IGimmick {
public:
    enum class Mode { SingleColorTutorial, Normal };

private:
    struct ColorFloor {
        RouteColor color = RouteColor::Red;
        Vector3 position{};
        int32_t orderNumber = 1;
        bool cleared = false;
        std::unique_ptr<Collision::Collider> collider;
        EmitterHandle floorEmitter;
        std::vector<std::unique_ptr<Model>> orderMarkers;
    };

    static inline bool singleColorTutorialCleared_ = false;

    GimmickContext context_{};
    GimmickState state_ = GimmickState::Ready;
    Mode mode_ = Mode::Normal;

    std::array<RouteColor, 4> colorOrder_{};
    int32_t colorCount_ = 4;
    int32_t nextFloorIndex_ = 0;
    std::vector<ColorFloor> floors_;
    Vector3 towerPosition_{};

    float timeLimitSeconds_ = 10.0f;
    float floorRadius_ = 1.5f;
    float floorOpacity_ = 0.55f;
    float minFloorDistance_ = 6.0f;
    float minTowerDistance_ = 4.0f;
    float placementRadius_ = 18.0f;

    float orderMarkerRadius_ = 0.22f;
    float orderMarkerSpacing_ = 0.5f;
    float orderMarkerHeight_ = 1.4f;

    bool pendingAdvanceToNormal_ = false;
    bool debugTuningPaused_ = false;

public:
    ~RouteGimmick() override;

    void Initialize(const GimmickContext& _context) override;
    void Update(float _deltaTime) override;
    void Draw() const override;
    GimmickType GetType() const override { return GimmickType::Route; }
    GimmickState GetState() const override { return state_; }
    float GetTimeLimitSeconds() const override { return timeLimitSeconds_; }
    void OnTimeLimitExpired() override { Finish(GimmickState::Failed); }

    Mode GetMode() const { return mode_; }

    void Debug() override;

    static void ResetTutorialProgress() {
        singleColorTutorialCleared_ = false;
    }

private:
    void LoadConfig();
    void SaveConfig() const;
    void DetermineMode();
    void GenerateColorOrder();
    void PlaceFloors();
    void CreateOrderMarkers(ColorFloor& _floor);
    void UpdateOrderMarkerTransforms(ColorFloor& _floor);
    void RefreshFloorVisuals();
    Vector3 GenerateFloorPosition() const;
    void OnFloorEntered(std::size_t _index);
    void AdvanceToNormal();
    void RegisterParticleTemplates() const;
    void EmitFloorClear(RouteColor _color, const Vector3& _position) const;
    void Finish(GimmickState _result);
};

#endif // ROUTE_GIMMICK_HPP_
