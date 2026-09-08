#include "RouteGimmick.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

#include "Collision/CollisionAttribute.hpp"
#include "GameObject/Player/Player.h"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Tower/MainTower.hpp"
#include "Tower/TowerManager.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

#ifdef _DEBUG
#include "DebugUIWidgets.hpp"
#include "imgui.h"
#endif

namespace {
    constexpr const char* REVEAL_SPAWN_FUNC_KEY = "RouteColorRevealSpawn";
    constexpr const char* FLOOR_CLEAR_SPAWN_FUNC_KEY = "RouteColorFloorClearSpawn";

    const char* TemplateNameFor(RouteColor _color) {
        switch (_color) {
            case RouteColor::Red: return "RouteColorReveal_Red";
            case RouteColor::Blue: return "RouteColorReveal_Blue";
            case RouteColor::Green: return "RouteColorReveal_Green";
            case RouteColor::Yellow: return "RouteColorReveal_Yellow";
        }
        return "RouteColorReveal_Red";
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

void RouteGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
    elapsedTime_ = 0.0f;
    nextFloorIndex_ = 0;
    revealedColorCount_ = 0;
    colorRevealTimer_ = 0.0f;
    guideNumberActive_ = false;
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
    StartColorRevealStep();

    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(true);
}

void RouteGimmick::Update(float _deltaTime) {
    if (state_ != GimmickState::Active) return;
    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    elapsedTime_ += _deltaTime;
    if (elapsedTime_ >= timeLimitSeconds_) {
        Finish(GimmickState::Failed);
        return;
    }

    UpdateColorReveal(_deltaTime);
    UpdateFloors(_deltaTime);

    if (pendingAdvanceToGuidedTutorial_) {
        const bool stillDisappearing = std::any_of(floors_.begin(), floors_.end(),
            [](const ColorFloor& _floor) { return _floor.cleared && _floor.model; });
        if (!stillDisappearing) {
            pendingAdvanceToGuidedTutorial_ = false;
            AdvanceToGuidedTutorial();
        }
    }
}

void RouteGimmick::Draw() const {
    for (const ColorFloor& floor : floors_) {
        if (floor.model) floor.model->Draw();
    }
    guideNumberText_.Draw();
}

void RouteGimmick::Debug() {
#ifdef _DEBUG
    ImGui::Begin("RouteGimmick");
    ImGui::Text("Mode: %s", mode_ == Mode::SingleColorTutorial ? "SingleColorTutorial"
        : mode_ == Mode::GuidedTutorial ? "GuidedTutorial" : "Normal");
    ImGui::Text("State: %s", state_ == GimmickState::Active ? "Active"
        : state_ == GimmickState::Success ? "Success"
        : state_ == GimmickState::Failed ? "Failed" : "Ready");
    ImGui::Text("Elapsed: %.2f / %.2f", elapsedTime_, timeLimitSeconds_);
    ImGui::Text("Progress: %d / %d", nextFloorIndex_, colorCount_);

    DebugUIWidgets::DragFloat("Time Limit", &timeLimitSeconds_, 0.1f, 5.0f, 60.0f);
    DebugUIWidgets::DragFloat("Color Reveal Interval", &colorRevealInterval_, 0.02f, 0.1f, 3.0f);
    DebugUIWidgets::DragFloat("Guided Color Reveal Interval", &guidedColorRevealInterval_, 0.02f, 0.3f, 4.0f);
    DebugUIWidgets::DragFloat("Guide Number Lead Seconds", &guideNumberLeadSeconds_, 0.01f, 0.1f, 2.0f);
    DebugUIWidgets::DragFloat("Min Floor Distance", &minFloorDistance_, 0.1f, 1.0f, 20.0f);
    DebugUIWidgets::DragFloat("Min Tower Distance", &minTowerDistance_, 0.1f, 1.0f, 20.0f);
    DebugUIWidgets::DragFloat("Placement Radius", &placementRadius_, 0.2f, 5.0f, 40.0f);
    DebugUIWidgets::DragFloat("Floor Disappear Duration", &floorDisappearDuration_, 0.01f, 0.05f, 2.0f);

    bool floorAppearanceChanged = false;
    floorAppearanceChanged |= DebugUIWidgets::DragFloat("Floor Radius", &floorRadius_, 0.02f, 0.3f, 4.0f);
    floorAppearanceChanged |= DebugUIWidgets::DragFloat("Floor Opacity", &floorOpacity_, 0.01f, 0.1f, 1.0f);
    if (floorAppearanceChanged) {
        for (ColorFloor& floor : floors_) {
            if (floor.cleared) continue;
            if (floor.collider) floor.collider->SetSize(Collision::SphereShape(floorRadius_));
            if (floor.model) {
                Vector4 tint = ColorToVector4(floor.color);
                tint.w = floorOpacity_;
                floor.model->SetColor(tint);
                floor.model->SetScale({floorRadius_, floorRadius_, floorRadius_});
                floor.model->Update();
            }
        }
    }

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
    floorDisappearDuration_ = read(tuning->second, "FloorDisappearDuration", floorDisappearDuration_);
    minFloorDistance_ = read(tuning->second, "MinFloorDistance", minFloorDistance_);
    minTowerDistance_ = read(tuning->second, "MinTowerDistance", minTowerDistance_);
    placementRadius_ = read(tuning->second, "PlacementRadius", placementRadius_);
    colorRevealInterval_ = read(tuning->second, "ColorRevealInterval", colorRevealInterval_);
    guidedColorRevealInterval_ = read(
        tuning->second, "GuidedColorRevealInterval", guidedColorRevealInterval_);
    guideNumberLeadSeconds_ = read(tuning->second, "GuideNumberLeadSeconds", guideNumberLeadSeconds_);
}

void RouteGimmick::SaveConfig() const {
    const auto json = Singleton<JsonParams>::GetInstance();
    json->SetValue("Route", "Tuning", "TimeLimitSeconds", timeLimitSeconds_);
    json->SetValue("Route", "Tuning", "FloorRadius", floorRadius_);
    json->SetValue("Route", "Tuning", "FloorOpacity", floorOpacity_);
    json->SetValue("Route", "Tuning", "FloorDisappearDuration", floorDisappearDuration_);
    json->SetValue("Route", "Tuning", "MinFloorDistance", minFloorDistance_);
    json->SetValue("Route", "Tuning", "MinTowerDistance", minTowerDistance_);
    json->SetValue("Route", "Tuning", "PlacementRadius", placementRadius_);
    json->SetValue("Route", "Tuning", "ColorRevealInterval", colorRevealInterval_);
    json->SetValue("Route", "Tuning", "GuidedColorRevealInterval", guidedColorRevealInterval_);
    json->SetValue("Route", "Tuning", "GuideNumberLeadSeconds", guideNumberLeadSeconds_);
    json->Save("Gimmick", "Route");
}

void RouteGimmick::DetermineMode() {
    if (!singleColorTutorialCleared_) {
        mode_ = Mode::SingleColorTutorial;
        colorCount_ = 1;
        return;
    }
    mode_ = guidedTutorialCleared_ ? Mode::Normal : Mode::GuidedTutorial;
    colorCount_ = 4;
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

        Vector4 tint = ColorToVector4(floor.color);
        tint.w = floorOpacity_;

        floor.model = std::make_unique<Model>();
        floor.model->Initialize("sphere");
        floor.model->SetTexture("white_x16.png");
        floor.model->SetColor(tint);
        floor.model->SetTranslate(floor.position);
        floor.model->SetScale({floorRadius_, floorRadius_, floorRadius_});
        floor.model->Update();
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

void RouteGimmick::UpdateFloors(float _deltaTime) {
    for (ColorFloor& floor : floors_) {
        if (floor.cleared) UpdateFloorDisappear(floor, _deltaTime);
    }
}

void RouteGimmick::UpdateFloorDisappear(ColorFloor& _floor, float _deltaTime) {
    if (!_floor.model) return;

    _floor.disappearTime += _deltaTime;
    const float t = std::clamp(_floor.disappearTime / floorDisappearDuration_, 0.0f, 1.0f);
    const float scale = floorRadius_ * (1.0f - t);

    Vector4 tint = ColorToVector4(_floor.color);
    tint.w = floorOpacity_ * (1.0f - t);
    _floor.model->SetColor(tint);
    _floor.model->SetScale({scale, scale, scale});
    _floor.model->Update();

    if (t >= 1.0f) _floor.model.reset();
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
    floor.disappearTime = 0.0f;
    if (floor.collider) floor.collider->Disable();
    EmitFloorClear(floor.color, floor.position);
    ++nextFloorIndex_;
    if (nextFloorIndex_ >= colorCount_) {
        if (mode_ == Mode::SingleColorTutorial) pendingAdvanceToGuidedTutorial_ = true;
        else Finish(GimmickState::Success);
    }
}

void RouteGimmick::AdvanceToGuidedTutorial() {
    singleColorTutorialCleared_ = true;
    mode_ = guidedTutorialCleared_ ? Mode::Normal : Mode::GuidedTutorial;
    colorCount_ = 4;

    elapsedTime_ = 0.0f;
    nextFloorIndex_ = 0;
    revealedColorCount_ = 0;
    colorRevealTimer_ = 0.0f;
    guideNumberActive_ = false;
    floors_.clear();

    GenerateColorOrder();
    PlaceFloors();
    StartColorRevealStep();
}

void RouteGimmick::UpdateColorReveal(float _deltaTime) {
    if (revealedColorCount_ >= colorCount_) return;

    colorRevealTimer_ += _deltaTime;

    if (mode_ == Mode::GuidedTutorial) {
        if (guideNumberActive_) {
            if (colorRevealTimer_ < guideNumberLeadSeconds_) return;
            HideGuideNumber();
            EmitColorReveal(colorOrder_[revealedColorCount_]);
            ++revealedColorCount_;
            return;
        }
        if (colorRevealTimer_ >= guidedColorRevealInterval_) StartColorRevealStep();
        return;
    }

    if (colorRevealTimer_ >= colorRevealInterval_) StartColorRevealStep();
}

void RouteGimmick::StartColorRevealStep() {
    if (revealedColorCount_ >= colorCount_) return;

    colorRevealTimer_ = 0.0f;
    if (mode_ == Mode::GuidedTutorial) {
        ShowGuideNumber(revealedColorCount_ + 1);
        return;
    }

    EmitColorReveal(colorOrder_[revealedColorCount_]);
    ++revealedColorCount_;
}

void RouteGimmick::ShowGuideNumber(int32_t _step) {
    guideNumberActive_ = true;

    const auto screen = Singleton<Screen>::GetInstance();
    const float centerX = screen->Width() * 0.5f - 10.0f;
    const float centerY = screen->Height() * 0.32f;

    if (guideNumberText_.GetText().empty()) {
        guideNumberText_.Initialize(std::to_string(_step), centerX, centerY, 36.0f);
        guideNumberText_.SetColor({0.9f, 0.9f, 0.9f, 1.0f});
    } else {
        guideNumberText_.SetText(std::to_string(_step));
        guideNumberText_.SetPosition(centerX, centerY);
    }
    guideNumberText_.SetVisible(true);
}

void RouteGimmick::HideGuideNumber() {
    guideNumberActive_ = false;
    guideNumberText_.SetVisible(false);
}

void RouteGimmick::RegisterParticleTemplates() const {
    if (!context_.particleSystem) return;

    context_.particleSystem->RegisterSpawnFunc(REVEAL_SPAWN_FUNC_KEY,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
            const Vector3 tangent{-direction.z, 0.0f, direction.x};
            _position = _center + direction * MathUtils::Random(0.2f, 0.4f);
            _velocity = tangent * MathUtils::Random(1.5f, 2.5f)
                + Vector3{0.0f, MathUtils::Random(1.5f, 2.5f), 0.0f};
        });

    context_.particleSystem->RegisterSpawnFunc(FLOOR_CLEAR_SPAWN_FUNC_KEY,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const Vector3 direction{std::cos(angle), 0.0f, std::sin(angle)};
            _position = _center + direction * MathUtils::Random(0.3f, 0.6f);
            _velocity = direction * MathUtils::Random(1.5f, 2.5f)
                + Vector3{0.0f, MathUtils::Random(0.05f, 0.2f), 0.0f};
        });

    for (RouteColor color : {RouteColor::Red, RouteColor::Blue, RouteColor::Green, RouteColor::Yellow}) {
        const Vector4 tint = ColorToVector4(color);
        const std::vector<GradientKey<Vector4>> fadeOut = {
            GradientKey<Vector4>{0.0f, tint},
            GradientKey<Vector4>{1.0f, {tint.x, tint.y, tint.z, 0.0f}}
        };

        ParticleSystem::EmitterConfig reveal;
        reveal.texture = "white_x16.png";
        reveal.frequency = 0.0f;
        reveal.duration = 0.0f;
        reveal.spawnCount = 16;
        reveal.size = {0.3f, 0.3f, 0.3f};
        reveal.particleLifetime = 0.5f;
        reveal.spawnFuncKey = REVEAL_SPAWN_FUNC_KEY;
        reveal.colorKeys = fadeOut;
        reveal.canvasName = "Main";

        ParticleSystem::Template revealTemplate;
        revealTemplate.emitters.push_back(reveal);
        context_.particleSystem->Register(TemplateNameFor(color), revealTemplate, true);

        ParticleSystem::EmitterConfig floorClear;
        floorClear.texture = "white_x16.png";
        floorClear.frequency = 0.0f;
        floorClear.duration = 0.0f;
        floorClear.spawnCount = 16;
        floorClear.size = {0.3f, 0.3f, 0.3f};
        floorClear.particleLifetime = 1.1f;
        floorClear.spawnFuncKey = FLOOR_CLEAR_SPAWN_FUNC_KEY;
        floorClear.colorKeys = fadeOut;
        floorClear.canvasName = "Main";

        ParticleSystem::Template floorClearTemplate;
        floorClearTemplate.emitters.push_back(floorClear);
        context_.particleSystem->Register(FloorClearTemplateNameFor(color), floorClearTemplate, true);
    }
}

void RouteGimmick::EmitColorReveal(RouteColor _color) const {
    if (!context_.particleSystem || !context_.player) return;
    context_.particleSystem->Emit(
        TemplateNameFor(_color), context_.player->GetPosition() + Vector3{0.0f, 2.2f, 0.0f});
}

void RouteGimmick::EmitFloorClear(RouteColor _color, const Vector3& _position) const {
    if (!context_.particleSystem) return;
    context_.particleSystem->Emit(FloorClearTemplateNameFor(_color), _position);
}

void RouteGimmick::Finish(GimmickState _result) {
    state_ = _result;
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(false);

    if (mode_ == Mode::SingleColorTutorial) singleColorTutorialCleared_ = true;
    else if (mode_ == Mode::GuidedTutorial) guidedTutorialCleared_ = true;
}
