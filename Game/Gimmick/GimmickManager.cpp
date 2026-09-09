#define NOMINMAX
#include "GimmickManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <variant>

#include "Gimmick/RouteGimmick.hpp"
#include "Gimmick/TowerDefenseGimmick.hpp"
#include "Gimmick/TowerOrbitGimmick.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"

#ifdef _DEBUG
#include "imgui.h"
#endif

#undef min
#undef max

namespace {
    const std::string WHITE_TEXTURE = "white_x16.png";

    Vector4 WithOpacity(Vector4 _color, float _opacity) {
        _color.w *= _opacity;
        return _color;
    }
}

void GimmickManager::Initialize(const GimmickContext& _context) {
    context_ = _context;
    activeGimmick_.reset();
    spawnTime_ = 0.0f;
    remainingTimeSeconds_ = 0.0f;
    lastRandomGimmick_.reset();
    LoadConfig();
    InitializeTimerGauge();
    RouteGimmick::ResetTutorialProgress();
}

void GimmickManager::Update(float _deltaTime) {
    if (pendingStart_) {
        const GimmickType type = *pendingStart_;
        pendingStart_.reset();
        StartGimmick(type);
    }

    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;
    if (debugPaused_) return;

    if (activeGimmick_) {
        activeGimmick_->Update(_deltaTime);
        if (!activeGimmick_->IsFinished()) {
            remainingTimeSeconds_ = std::max(remainingTimeSeconds_ - _deltaTime, 0.0f);
            if (remainingTimeSeconds_ <= 0.0f) activeGimmick_->OnTimeLimitExpired();
        }
        if (activeGimmick_->IsFinished()) {
            activeGimmick_.reset();
            remainingTimeSeconds_ = 0.0f;
            spawnTime_ = 0.0f;
        }
        UpdateTimerGauge();
        return;
    }

    spawnTime_ += _deltaTime;
    if (spawnTime_ < spawnInterval_) return;
    spawnTime_ = 0.0f;
    StartRandomGimmick();
}

void GimmickManager::Draw() {
    if (activeGimmick_) activeGimmick_->Draw();
    if (!activeGimmick_ || !visible_) return;
    timerGaugeFrame_.Draw();
    timerGaugeFill_.Draw();
    timerLabelText_.Draw();
    timerValueText_.Draw();
}

void GimmickManager::Debug() {
#ifdef _DEBUG
    ImGui::Begin("GimmickManager");
    ImGui::Text("Active: %s", activeGimmick_ ? "Yes" : "No");
    if (activeGimmick_) {
        ImGui::Text("Remaining: %.2f / %.2f", remainingTimeSeconds_, timeLimitSeconds_);
    }

    if (ImGui::Button("Route")) pendingStart_ = GimmickType::Route;
    ImGui::SameLine();
    if (ImGui::Button("TowerDefense")) pendingStart_ = GimmickType::TowerDefense;
    ImGui::SameLine();
    if (ImGui::Button("TowerOrbit")) pendingStart_ = GimmickType::TowerOrbit;

    if (activeGimmick_) {
        ImGui::Checkbox("Paused", &debugPaused_);
        ImGui::SameLine();
        if (ImGui::Button("Restart")) pendingStart_ = activeGimmick_->GetType();
    }
    ImGui::End();

    if (activeGimmick_) activeGimmick_->Debug();
#endif
}

void GimmickManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Gimmick")) return;
    const auto groups = json->GetGroups("Gimmick");

    const auto read = [](const auto& _group, const char* _key, float _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto floatValue = std::get_if<float>(&entry->second)) return *floatValue;
        if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*integerValue);
        }
        return _fallback;
    };

    if (const auto spawn = groups.find("Spawn"); spawn != groups.end()) {
        spawnInterval_ = read(spawn->second, "IntervalSeconds", spawnInterval_);
    }
    if (const auto lottery = groups.find("Lottery"); lottery != groups.end()) {
        routeWeight_ = read(lottery->second, "RouteWeight", routeWeight_);
        towerDefenseWeight_ = read(
            lottery->second, "TowerDefenseWeight", towerDefenseWeight_);
        towerOrbitWeight_ = read(lottery->second, "TowerOrbitWeight", towerOrbitWeight_);
    }
    if (const auto gauge = groups.find("TimerGauge"); gauge != groups.end()) {
        const auto readValue = []<typename T>(
            const auto& _group, const char* _key, const T& _fallback) {
            const auto entry = _group.find(_key);
            if (entry == _group.end()) return _fallback;
            if (const auto value = std::get_if<T>(&entry->second)) return *value;
            return _fallback;
        };
        timerGaugePosition_ = readValue(gauge->second, "Position", timerGaugePosition_);
        timerGaugeSize_ = readValue(gauge->second, "Size", timerGaugeSize_);
        timerGaugeFrameThickness_ = read(
            gauge->second, "FrameThickness", timerGaugeFrameThickness_);
        timerGaugeFrameColor_ = readValue(
            gauge->second, "FrameColor", timerGaugeFrameColor_);
        timerGaugeColor_ = readValue(gauge->second, "Color", timerGaugeColor_);
        timerLabel_ = readValue(gauge->second, "Label", timerLabel_);
        timerLabelPosition_ = readValue(
            gauge->second, "LabelPosition", timerLabelPosition_);
        timerLabelFontSize_ = read(gauge->second, "LabelFontSize", timerLabelFontSize_);
        timerLabelColor_ = readValue(gauge->second, "LabelColor", timerLabelColor_);
        timerValueRightX_ = read(gauge->second, "ValueRightX", timerValueRightX_);
        timerValuePositionY_ = read(
            gauge->second, "ValuePositionY", timerValuePositionY_);
        timerValueFontSize_ = read(
            gauge->second, "ValueFontSize", timerValueFontSize_);
    }

    spawnInterval_ = std::isfinite(spawnInterval_) ? std::max(spawnInterval_, 0.0f) : 30.0f;
    routeWeight_ = std::isfinite(routeWeight_) ? std::max(routeWeight_, 0.0f) : 1.0f;
    towerDefenseWeight_ = std::isfinite(towerDefenseWeight_)
        ? std::max(towerDefenseWeight_, 0.0f) : 1.0f;
    towerOrbitWeight_ = std::isfinite(towerOrbitWeight_)
        ? std::max(towerOrbitWeight_, 0.0f) : 1.0f;
}

void GimmickManager::StartRandomGimmick() {
    // 前回選ばれた種類だけ今回の候補から外し、同じギミックの連続発生を防ぐ。
    const float routeWeight = lastRandomGimmick_ != GimmickType::Route
        ? routeWeight_ : 0.0f;
    const float towerDefenseWeight = lastRandomGimmick_ != GimmickType::TowerDefense
        ? towerDefenseWeight_ : 0.0f;
    const float towerOrbitWeight = lastRandomGimmick_ != GimmickType::TowerOrbit
        ? towerOrbitWeight_ : 0.0f;
    const float totalWeight = routeWeight + towerDefenseWeight + towerOrbitWeight;
    if (totalWeight <= 0.0f) return;

    const float lottery = MathUtils::Random(0.0f, totalWeight);
    GimmickType type = GimmickType::TowerOrbit;
    if (lottery < routeWeight) {
        type = GimmickType::Route;
    } else if (lottery < routeWeight + towerDefenseWeight) {
        type = GimmickType::TowerDefense;
    }

    lastRandomGimmick_ = type;
    StartGimmick(type);
}

void GimmickManager::StartGimmick(GimmickType _type) {
    activeGimmick_ = CreateGimmick(_type);
    if (activeGimmick_) {
        activeGimmick_->Initialize(context_);
        timeLimitSeconds_ = activeGimmick_->GetTimeLimitSeconds();
        if (!std::isfinite(timeLimitSeconds_) || timeLimitSeconds_ <= 0.0f) {
            timeLimitSeconds_ = 10.0f;
        }
        remainingTimeSeconds_ = timeLimitSeconds_;
        UpdateTimerGauge();
    }
    spawnTime_ = 0.0f;
}

std::unique_ptr<IGimmick> GimmickManager::CreateGimmick(GimmickType _type) const {
    switch (_type) {
    case GimmickType::Route:
        return std::make_unique<RouteGimmick>();
    case GimmickType::TowerDefense:
        return std::make_unique<TowerDefenseGimmick>();
    case GimmickType::TowerOrbit:
        return std::make_unique<TowerOrbitGimmick>();
    }
    return nullptr;
}

void GimmickManager::InitializeTimerGauge() {
    for (Sprite* sprite : {&timerGaugeFrame_, &timerGaugeFill_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }
    timerLabelText_.Initialize(
        timerLabel_, timerLabelPosition_.x, timerLabelPosition_.y, timerLabelFontSize_);
    timerValueText_.Initialize("", timerValueRightX_, timerValuePositionY_, timerValueFontSize_);
    UpdateTimerGauge();
}

void GimmickManager::UpdateTimerGauge() {
    const float ratio = timeLimitSeconds_ > 0.0f
        ? std::clamp(remainingTimeSeconds_ / timeLimitSeconds_, 0.0f, 1.0f)
        : 0.0f;
    timerGaugeFrame_.SetPosition({
        timerGaugePosition_.x - timerGaugeFrameThickness_, timerGaugePosition_.y});
    timerGaugeFrame_.SetSize({
        timerGaugeSize_.x + timerGaugeFrameThickness_ * 2.0f,
        timerGaugeSize_.y + timerGaugeFrameThickness_ * 2.0f});
    timerGaugeFrame_.SetColor(WithOpacity(timerGaugeFrameColor_, opacity_));
    timerGaugeFrame_.Update();

    timerGaugeFill_.SetPosition(timerGaugePosition_);
    timerGaugeFill_.SetSize({timerGaugeSize_.x * ratio, timerGaugeSize_.y});
    timerGaugeFill_.SetColor(WithOpacity(timerGaugeColor_, opacity_));
    timerGaugeFill_.Update();

    timerLabelText_.SetColor(WithOpacity(timerLabelColor_, opacity_));
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.1fs", remainingTimeSeconds_);
    const std::string value = buffer;
    timerValueText_.SetText(value);
    timerValueText_.SetPosition(
        timerValueRightX_
            - static_cast<float>(value.size()) * timerValueFontSize_ * timerValueCharWidthRatio_,
        timerValuePositionY_);
    timerValueText_.SetColor(WithOpacity(timerGaugeColor_, opacity_));
}

void GimmickManager::SetVisible(bool _visible) {
    visible_ = _visible;
    timerLabelText_.SetVisible(_visible);
    timerValueText_.SetVisible(_visible);
}

void GimmickManager::SetOpacity(float _opacity) {
    opacity_ = std::clamp(_opacity, 0.0f, 1.0f);
    UpdateTimerGauge();
}

bool GimmickManager::GetIndicatorPosition(Vector3& _position) const {
    return activeGimmick_ && activeGimmick_->GetIndicatorPosition(_position);
}
