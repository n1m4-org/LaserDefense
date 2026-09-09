#include "RouteGimmick.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

#include "Collision/CollisionAttribute.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Tower/MainTower.hpp"
#include "Tower/TowerManager.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

#ifdef _DEBUG
#include "DebugUIWidgets.hpp"
#include "imgui.h"
#endif

#undef min
#undef max

namespace {
    constexpr const char* FLOOR_AREA_SPAWN_FUNC_KEY = "RouteColorFloorAreaSpawn";
    constexpr const char* FLOOR_CLEAR_SPAWN_FUNC_KEY = "RouteColorFloorClearSpawn";
    constexpr const char* FLOOR_CLEAR_UPDATE_FUNC_KEY = "RouteColorFloorClearUpdate";

    const char* FloorAreaTemplateNameFor(RouteColor _color) {
        switch (_color) {
            case RouteColor::Red: return "RouteColorFloorArea_Red";
            case RouteColor::Blue: return "RouteColorFloorArea_Blue";
            case RouteColor::Green: return "RouteColorFloorArea_Green";
            case RouteColor::Yellow: return "RouteColorFloorArea_Yellow";
        }
        return "RouteColorFloorArea_Red";
    }

    const char* FloorClearTemplateNameFor(RouteColor _color) {
        switch (_color) {
            case RouteColor::Red: return "RouteColorFloorClear_Red";
            case RouteColor::Blue: return "RouteColorFloorClear_Blue";
            case RouteColor::Green: return "RouteColorFloorClear_Green";
            case RouteColor::Yellow: return "RouteColorFloorClear_Yellow";
        }
        return "RouteColorFloorClear_Red";
    }

    Vector4 ColorToVector4(RouteColor _color) {
        switch (_color) {
            case RouteColor::Red: return {1.0f, 0.15f, 0.15f, 1.0f};
            case RouteColor::Blue: return {0.2f, 0.4f, 1.0f, 1.0f};
            case RouteColor::Green: return {0.2f, 1.0f, 0.3f, 1.0f};
            case RouteColor::Yellow: return {1.0f, 0.9f, 0.1f, 1.0f};
        }
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

RouteGimmick::~RouteGimmick() {
    for (ColorFloor& floor : floors_) {
        floor.floorEmitter.Stop();
    }
}

void RouteGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
    nextFloorIndex_ = 0;
    floors_.clear();

    LoadConfig();

    towerPosition_ = {};
    if (context_.towerManager && !context_.towerManager->GetMainTowers().empty()) {
        towerPosition_ = context_.towerManager->GetMainTowers().front()->GetPosition();
    }

    DetermineMode();
    GenerateColorOrder();
    RegisterParticleTemplates();
    PlaceFloors();

    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(true);
}

void RouteGimmick::Update(float _deltaTime) {
    if (state_ != GimmickState::Active) return;

    if (pendingAdvanceToNormal_) {
        pendingAdvanceToNormal_ = false;
        AdvanceToNormal();
    }

    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;
    if (debugTuningPaused_) return;

}

void RouteGimmick::Draw() const {
    for (const ColorFloor& floor : floors_) {
        for (const auto& marker : floor.orderMarkers) marker->Draw();
    }
}

void RouteGimmick::Debug() {
#ifdef _DEBUG
    ImGui::Begin("RouteGimmick");
    ImGui::Text("Mode: %s", mode_ == Mode::SingleColorTutorial ? "SingleColorTutorial" : "Normal");
    ImGui::Text("State: %s", state_ == GimmickState::Active ? "Active"
        : state_ == GimmickState::Success ? "Success"
        : state_ == GimmickState::Failed ? "Failed" : "Ready");
    ImGui::Text("Time Limit: %.2f", timeLimitSeconds_);
    ImGui::Text("Progress: %d / %d", nextFloorIndex_, colorCount_);

    DebugUIWidgets::Checkbox("Pause While Tuning", &debugTuningPaused_);

    DebugUIWidgets::DragFloat("Time Limit", &timeLimitSeconds_, 0.1f, 5.0f, 60.0f);
    DebugUIWidgets::DragFloat("Min Floor Distance", &minFloorDistance_, 0.1f, 1.0f, 20.0f);
    DebugUIWidgets::DragFloat("Min Tower Distance", &minTowerDistance_, 0.1f, 1.0f, 20.0f);
    DebugUIWidgets::DragFloat("Placement Radius", &placementRadius_, 0.2f, 5.0f, 40.0f);

    // ドラッグ中は毎フレーム変更イベントが発火するため、RefreshFloorVisuals()は
    // 値が実際に変わった時ではなく、編集操作が確定した瞬間(マウスを離す等)にのみ呼ぶ。
    bool visualsChanged = false;
    DebugUIWidgets::DragFloat("Floor Radius", &floorRadius_, 0.02f, 0.3f, 4.0f);
    visualsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    DebugUIWidgets::DragFloat("Floor Opacity", &floorOpacity_, 0.01f, 0.1f, 1.0f);
    visualsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    DebugUIWidgets::DragFloat("Order Marker Radius", &orderMarkerRadius_, 0.02f, 0.02f, 2.0f);
    visualsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    DebugUIWidgets::DragFloat("Order Marker Spacing", &orderMarkerSpacing_, 0.02f, 0.05f, 3.0f);
    visualsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    DebugUIWidgets::DragFloat("Order Marker Height", &orderMarkerHeight_, 0.05f, 0.1f, 6.0f);
    visualsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    if (visualsChanged) RefreshFloorVisuals();

    if (ImGui::Button("Save Tuning")) SaveConfig();
    ImGui::End();
#endif
}

void RouteGimmick::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Route")) return;
    const auto groups = json->GetGroups("Route");
    const auto tuning = groups.find("Tuning");
    if (tuning == groups.end()) return;

    const auto read = [](const auto& _group, const char* _key, float _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto value = std::get_if<float>(&entry->second)) return *value;
        if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*integerValue);
        }
        return _fallback;
    };

    timeLimitSeconds_ = read(tuning->second, "TimeLimitSeconds", timeLimitSeconds_);
    floorRadius_ = read(tuning->second, "FloorRadius", floorRadius_);
    floorOpacity_ = read(tuning->second, "FloorOpacity", floorOpacity_);
    minFloorDistance_ = read(tuning->second, "MinFloorDistance", minFloorDistance_);
    minTowerDistance_ = read(tuning->second, "MinTowerDistance", minTowerDistance_);
    placementRadius_ = read(tuning->second, "PlacementRadius", placementRadius_);
    orderMarkerRadius_ = read(tuning->second, "OrderMarkerRadius", orderMarkerRadius_);
    orderMarkerSpacing_ = read(tuning->second, "OrderMarkerSpacing", orderMarkerSpacing_);
    orderMarkerHeight_ = read(tuning->second, "OrderMarkerHeight", orderMarkerHeight_);
}

void RouteGimmick::SaveConfig() const {
    const auto json = Singleton<JsonParams>::GetInstance();
    json->SetValue("Route", "Tuning", "TimeLimitSeconds", timeLimitSeconds_);
    json->SetValue("Route", "Tuning", "FloorRadius", floorRadius_);
    json->SetValue("Route", "Tuning", "FloorOpacity", floorOpacity_);
    json->SetValue("Route", "Tuning", "MinFloorDistance", minFloorDistance_);
    json->SetValue("Route", "Tuning", "MinTowerDistance", minTowerDistance_);
    json->SetValue("Route", "Tuning", "PlacementRadius", placementRadius_);
    json->SetValue("Route", "Tuning", "OrderMarkerRadius", orderMarkerRadius_);
    json->SetValue("Route", "Tuning", "OrderMarkerSpacing", orderMarkerSpacing_);
    json->SetValue("Route", "Tuning", "OrderMarkerHeight", orderMarkerHeight_);
    json->Save("Gimmick", "Route");
}

void RouteGimmick::DetermineMode() {
    mode_ = singleColorTutorialCleared_ ? Mode::Normal : Mode::SingleColorTutorial;
    colorCount_ = mode_ == Mode::SingleColorTutorial ? 1 : 4;
}

void RouteGimmick::GenerateColorOrder() {
    colorOrder_ = {RouteColor::Red, RouteColor::Blue, RouteColor::Green, RouteColor::Yellow};
    std::shuffle(colorOrder_.begin(), colorOrder_.end(), MathUtils::GetRandomEngine());
}

void RouteGimmick::PlaceFloors() {
    floors_.clear();
    floors_.reserve(colorCount_);
    for (int32_t i = 0; i < colorCount_; ++i) {
        ColorFloor floor;
        floor.color = colorOrder_[i];
        floor.orderNumber = i + 1;
        floor.position = GenerateFloorPosition();
        floors_.push_back(std::move(floor));
    }

    for (std::size_t i = 0; i < floors_.size(); ++i) {
        ColorFloor& floor = floors_[i];

        floor.collider = std::make_unique<Collision::Collider>();
        floor.collider->SetName("ColorFloor")
            ->SetType(Collision::Type::Sphere)
            ->SetOwner(this)
            ->AddAttribute(CollisionAttribute::Floor)
            ->AddIgnore(CollisionAttribute::Enemy | CollisionAttribute::Tower | CollisionAttribute::Laser)
            ->SetTranslate(floor.position)
            ->SetSize(Collision::SphereShape(floorRadius_))
            ->SetEvent(Collision::EventType::Trigger, [this, i](const Collision::Collider*) {
                OnFloorEntered(i);
            })
            ->Enable();

        if (context_.particleSystem) {
            floor.floorEmitter = context_.particleSystem->Emit(
                FloorAreaTemplateNameFor(floor.color), floor.position);
        }

        CreateOrderMarkers(floor);
    }
}

void RouteGimmick::CreateOrderMarkers(ColorFloor& _floor) {
    _floor.orderMarkers.clear();
    _floor.orderMarkers.reserve(_floor.orderNumber);

    for (int32_t i = 0; i < _floor.orderNumber; ++i) {
        auto marker = std::make_unique<Model>();
        marker->Initialize("sphere");
        marker->SetTexture("white_x16.png");
        marker->SetColor(ColorToVector4(_floor.color));
        _floor.orderMarkers.push_back(std::move(marker));
    }

    UpdateOrderMarkerTransforms(_floor);
}

void RouteGimmick::UpdateOrderMarkerTransforms(ColorFloor& _floor) {
    const float totalWidth = orderMarkerSpacing_ * static_cast<float>(_floor.orderNumber - 1);
    const float startX = -totalWidth * 0.5f;

    for (std::size_t i = 0; i < _floor.orderMarkers.size(); ++i) {
        _floor.orderMarkers[i]->SetTranslate(_floor.position
            + Vector3{startX + orderMarkerSpacing_ * static_cast<float>(i), orderMarkerHeight_, 0.0f});
        _floor.orderMarkers[i]->SetScale({orderMarkerRadius_, orderMarkerRadius_, orderMarkerRadius_});
        _floor.orderMarkers[i]->Update();
    }
}

void RouteGimmick::RefreshFloorVisuals() {
    RegisterParticleTemplates();

    for (ColorFloor& floor : floors_) {
        if (floor.cleared) continue;

        if (floor.collider) floor.collider->SetSize(Collision::SphereShape(floorRadius_));

        floor.floorEmitter.Stop();
        if (context_.particleSystem) {
            floor.floorEmitter = context_.particleSystem->Emit(
                FloorAreaTemplateNameFor(floor.color), floor.position);
        }

        UpdateOrderMarkerTransforms(floor);
    }
}

Vector3 RouteGimmick::GenerateFloorPosition() const {
    constexpr int32_t kMaxAttempts = 30;
    Vector3 candidate = towerPosition_;

    for (int32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
        const float radius = MathUtils::Random(minTowerDistance_, placementRadius_);
        candidate = towerPosition_
            + Vector3{std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};

        bool farEnough = true;
        for (const ColorFloor& other : floors_) {
            if (MathUtils::Distance(candidate, other.position) < minFloorDistance_) {
                farEnough = false;
                break;
            }
        }
        if (farEnough) return candidate;
    }
    return candidate;
}

void RouteGimmick::OnFloorEntered(std::size_t _index) {
    if (state_ != GimmickState::Active) return;

    ColorFloor& floor = floors_[_index];
    if (floor.cleared) return;

    if (floor.color != colorOrder_[nextFloorIndex_]) {
        Finish(GimmickState::Failed);
        return;
    }

    floor.cleared = true;
    if (floor.collider) floor.collider->Disable();
    floor.floorEmitter.Stop();
    floor.orderMarkers.clear();
    EmitFloorClear(floor.color, floor.position);
    ++nextFloorIndex_;
    if (nextFloorIndex_ >= colorCount_) {
        if (mode_ == Mode::SingleColorTutorial) pendingAdvanceToNormal_ = true;
        else Finish(GimmickState::Success);
    }
}

void RouteGimmick::AdvanceToNormal() {
    singleColorTutorialCleared_ = true;
    mode_ = Mode::Normal;
    colorCount_ = 4;

    nextFloorIndex_ = 0;
    floors_.clear();

    GenerateColorOrder();
    PlaceFloors();
}

void RouteGimmick::RegisterParticleTemplates() const {
    if (!context_.particleSystem) return;

    const float floorRadius = floorRadius_;
    context_.particleSystem->RegisterSpawnFunc(FLOOR_AREA_SPAWN_FUNC_KEY,
        [floorRadius](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
            _position = _center + direction * floorRadius + Vector3{0.0f, 0.6f, 0.0f};
            _velocity = {0.0f, MathUtils::Random(0.05f, 0.15f), 0.0f};
        });

    context_.particleSystem->RegisterUpdateFunc(FLOOR_CLEAR_UPDATE_FUNC_KEY,
        [](float, const Vector3&, Vector3&, Vector3& _velocity, Vector4&) {
            constexpr float DT = 1.0f / 60.0f;
            constexpr float ANGULAR_SPEED = MathUtils::F_PI * 1.0f;
            const float cosA = std::cos(ANGULAR_SPEED * DT);
            const float sinA = std::sin(ANGULAR_SPEED * DT);
            const float vx = _velocity.x;
            const float vz = _velocity.z;
            _velocity.x = vx * cosA - vz * sinA;
            _velocity.z = vx * sinA + vz * cosA;
        });

    context_.particleSystem->RegisterSpawnFunc(FLOOR_CLEAR_SPAWN_FUNC_KEY,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
            _position = _center + direction * MathUtils::Random(0.3f, 0.6f)
                + Vector3{0.0f, MathUtils::Random(0.6f, 0.9f), 0.0f};
            _velocity = direction * MathUtils::Random(1.5f, 2.5f)
                + Vector3{0.0f, MathUtils::Random(0.05f, 0.2f), 0.0f};
        });

    for (RouteColor color : {RouteColor::Red, RouteColor::Blue, RouteColor::Green, RouteColor::Yellow}) {
        const Vector4 tint = ColorToVector4(color);

        ParticleSystem::EmitterConfig floorArea;
        floorArea.texture = "white_x16.png";
        floorArea.frequency = 0.12f;
        floorArea.duration = 120.0f;
        floorArea.spawnCount = 2;
        floorArea.size = {0.25f, 0.25f, 0.25f};
        floorArea.particleLifetime = 1.0f;
        floorArea.spawnFuncKey = FLOOR_AREA_SPAWN_FUNC_KEY;
        floorArea.colorKeys = {
            GradientKey<Vector4>{0.0f, {tint.x, tint.y, tint.z, floorOpacity_}},
            GradientKey<Vector4>{1.0f, {tint.x, tint.y, tint.z, 0.0f}}
        };
        floorArea.canvasName = "Main";

        ParticleSystem::Template floorAreaTemplate;
        floorAreaTemplate.emitters.push_back(floorArea);
        context_.particleSystem->Register(FloorAreaTemplateNameFor(color), floorAreaTemplate, true);

        ParticleSystem::EmitterConfig floorClear;
        floorClear.texture = "white_x16.png";
        floorClear.frequency = 0.0f;
        floorClear.duration = 0.0f;
        floorClear.spawnCount = 16;
        floorClear.size = {0.3f, 0.3f, 0.3f};
        floorClear.particleLifetime = 1.1f;
        floorClear.spawnFuncKey = FLOOR_CLEAR_SPAWN_FUNC_KEY;
        floorClear.updateFuncKey = FLOOR_CLEAR_UPDATE_FUNC_KEY;
        floorClear.colorKeys = {
            GradientKey<Vector4>{0.0f, tint},
            GradientKey<Vector4>{1.0f, {tint.x, tint.y, tint.z, 0.0f}}
        };
        floorClear.canvasName = "Main";

        ParticleSystem::Template floorClearTemplate;
        floorClearTemplate.emitters.push_back(floorClear);
        context_.particleSystem->Register(FloorClearTemplateNameFor(color), floorClearTemplate, true);
    }
}

void RouteGimmick::EmitFloorClear(RouteColor _color, const Vector3& _position) const {
    if (!context_.particleSystem) return;
    context_.particleSystem->Emit(FloorClearTemplateNameFor(_color), _position);
}

void RouteGimmick::Finish(GimmickState _result) {
    state_ = _result;
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(false);

    for (ColorFloor& floor : floors_) {
        if (floor.collider) floor.collider->Disable();
        floor.floorEmitter.Stop();
    }

    if (mode_ == Mode::SingleColorTutorial) singleColorTutorialCleared_ = true;
}
