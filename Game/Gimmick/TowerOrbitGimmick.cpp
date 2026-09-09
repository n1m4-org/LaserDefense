#define NOMINMAX

#include "TowerOrbitGimmick.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "GameObject/Player/Player.h"
#include "Json/JsonParams.hpp"
#include "Laser/Laser.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Tower/Tower.hpp"
#include "Tower/TowerManager.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

namespace {
    constexpr const char* AMBIENT_TEMPLATE = "TowerOrbitAmbient";
    constexpr const char* AMBIENT_SPAWN = "TowerOrbitAmbientSpawn";
    constexpr const char* COMPLETE_TEMPLATE = "TowerOrbitComplete";
    constexpr const char* COMPLETE_SPAWN = "TowerOrbitCompleteSpawn";
    constexpr float AOE_HEIGHT = 0.04f;

    float EaseOutCubic(float _t) {
        const float inverse = 1.0f - _t;
        return 1.0f - inverse * inverse * inverse;
    }
}

TowerOrbitGimmick::~TowerOrbitGimmick() {
    ambientEffect_.Stop();
}

void TowerOrbitGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Ready;
    phase_ = Phase::Orbit;
    targetTower_ = nullptr;
    accumulatedAngle_ = 0.0f;
    previousAngle_ = 0.0f;
    hasPreviousAngle_ = false;
    isPlayerOrbiting_ = false;
    arrowRotation_ = 0.0f;
    completionElapsed_ = 0.0f;

    LoadConfig();
    SelectTargetTower();
    if (!context_.player || !context_.laser || !targetTower_) {
        state_ = GimmickState::Failed;
        return;
    }

    direction_ = MathUtils::Random(0.0f, 1.0f) < 0.5f
        ? OrbitDirection::Clockwise
        : OrbitDirection::CounterClockwise;
    effectColor_ = {0.15f, 1.0f, 0.35f, 1.0f};

    InitializeVisuals();
    InitializeParticles();
    state_ = GimmickState::Active;
}

void TowerOrbitGimmick::Update(float _deltaTime) {
    if (state_ != GimmickState::Active || !targetTower_ || !targetTower_->IsActive()) {
        if (state_ == GimmickState::Active) state_ = GimmickState::Failed;
        return;
    }
    if (!std::isfinite(_deltaTime) || _deltaTime < 0.0f) return;

    if (phase_ == Phase::Orbit) UpdateOrbit(_deltaTime);
    else UpdateCompletion(_deltaTime);
}

void TowerOrbitGimmick::Draw() const {
    if (state_ != GimmickState::Active) return;
    if (baseAoE_) baseAoE_->Draw();
    if (progressAoE_) progressAoE_->Draw();
    for (const auto& arrow : arrows_) {
        if (arrow) arrow->Draw();
    }
}

void TowerOrbitGimmick::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Gimmick")) return;
    const auto groups = json->GetGroups("Gimmick");
    const auto orbit = groups.find("TowerOrbit");
    if (orbit == groups.end()) return;

    const auto read = [&](const char* _key, float _fallback) {
        const auto entry = orbit->second.find(_key);
        if (entry == orbit->second.end()) return _fallback;
        if (const auto value = std::get_if<float>(&entry->second)) return *value;
        if (const auto value = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*value);
        }
        return _fallback;
    };

    requiredDegrees_ = std::max(read("RequiredDegrees", requiredDegrees_), 1.0f);
    timeLimitSeconds_ = std::max(read("TimeLimitSeconds", timeLimitSeconds_), 0.01f);
    aoeRadius_ = std::max(read("AoERadius", aoeRadius_), 0.1f);
    completionSeconds_ = std::max(
        read("CompletionSeconds", completionSeconds_), 0.01f);
    completionFlashSeconds_ = std::max(
        read("CompletionFlashSeconds", completionFlashSeconds_), 0.01f);
    completionBurstCount_ = static_cast<uint16_t>(std::clamp(
        read("CompletionBurstCount", static_cast<float>(completionBurstCount_)),
        1.0f, 1000.0f));
    completionBurstUpSpeedMin_ = std::max(
        read("CompletionBurstUpSpeedMin", completionBurstUpSpeedMin_), 0.0f);
    completionBurstUpSpeedMax_ = std::max(
        read("CompletionBurstUpSpeedMax", completionBurstUpSpeedMax_),
        completionBurstUpSpeedMin_);
    completionBurstHorizontalSpeed_ = std::max(
        read("CompletionBurstHorizontalSpeed", completionBurstHorizontalSpeed_), 0.0f);
    shockwaveScale_ = std::max(read("ShockwaveScale", shockwaveScale_), 1.0f);
    arrowRadiusRatio_ = std::max(read("ArrowRadiusRatio", arrowRadiusRatio_), 0.0f);
    arrowSize_ = std::max(read("ArrowSize", arrowSize_), 0.1f);
    arrowIdleSpeedDegrees_ = std::max(
        read("ArrowIdleSpeedDegrees", arrowIdleSpeedDegrees_), 0.0f);
    arrowActiveSpeedDegrees_ = std::max(
        read("ArrowActiveSpeedDegrees", arrowActiveSpeedDegrees_),
        arrowIdleSpeedDegrees_);
}

void TowerOrbitGimmick::SelectTargetTower() {
    if (!context_.towerManager) return;

    std::vector<Tower*> candidates;
    for (const auto& tower : context_.towerManager->GetTowers()) {
        if (tower && tower->IsActive()) candidates.push_back(tower.get());
    }
    if (candidates.empty()) return;

    const float randomIndex = MathUtils::Random(
        0.0f, static_cast<float>(candidates.size()) - 0.001f);
    const std::size_t index = std::min(
        static_cast<std::size_t>(std::max(randomIndex, 0.0f)), candidates.size() - 1);
    targetTower_ = candidates[index];
}

void TowerOrbitGimmick::InitializeVisuals() {
    const Vector3 center = targetTower_->GetPosition()
        + Vector3{0.0f, AOE_HEIGHT, 0.0f};

    baseAoE_ = std::make_unique<Model>();
    baseAoE_->Initialize("plane");
    baseAoE_->SetTexture("circle2.png");
    baseAoE_->SetTranslate(center);
    baseAoE_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    baseAoE_->SetScale({aoeRadius_, aoeRadius_, 1.0f});
    baseAoE_->SetColor({1.0f, 1.0f, 1.0f, 0.38f});
    baseAoE_->Update();

    progressAoE_ = std::make_unique<Model>();
    progressAoE_->Initialize("plane");
    progressAoE_->SetTexture("circle2.png");
    progressAoE_->SetTranslate(center + Vector3{0.0f, AOE_HEIGHT, 0.0f});
    progressAoE_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    progressAoE_->SetScale({0.01f, 0.01f, 1.0f});
    progressAoE_->SetColor({
        effectColor_.x, effectColor_.y, effectColor_.z, 0.72f});
    progressAoE_->Update();

    for (auto& arrow : arrows_) {
        arrow = std::make_unique<Model>();
        arrow->Initialize("plane");
        arrow->SetTexture("arrow.png");
        arrow->SetScale({arrowSize_, arrowSize_, 1.0f});
    }
    UpdateArrowVisuals();
}

void TowerOrbitGimmick::InitializeParticles() {
    if (!context_.particleSystem) return;

    const float effectRadius = aoeRadius_;
    context_.particleSystem->RegisterSpawnFunc(AMBIENT_SPAWN,
        [effectRadius](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const float radius = std::sqrt(MathUtils::Random(0.0f, 1.0f)) * effectRadius;
            _position = _center + Vector3{
                std::cos(angle) * radius,
                MathUtils::Random(0.05f, 0.35f),
                std::sin(angle) * radius};
            _velocity = {0.0f, MathUtils::Random(0.3f, 1.0f), 0.0f};
        });
    const float burstUpSpeedMin = completionBurstUpSpeedMin_;
    const float burstUpSpeedMax = completionBurstUpSpeedMax_;
    const float burstHorizontalSpeed = completionBurstHorizontalSpeed_;
    context_.particleSystem->RegisterSpawnFunc(COMPLETE_SPAWN,
        [effectRadius, burstUpSpeedMin, burstUpSpeedMax, burstHorizontalSpeed](
            const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const float radius = std::sqrt(MathUtils::Random(0.0f, 1.0f)) * effectRadius;
            _position = _center + Vector3{
                std::cos(angle) * radius,
                MathUtils::Random(0.05f, 0.45f),
                std::sin(angle) * radius};
            _velocity = {
                MathUtils::Random(-burstHorizontalSpeed, burstHorizontalSpeed),
                MathUtils::Random(burstUpSpeedMin, burstUpSpeedMax),
                MathUtils::Random(-burstHorizontalSpeed, burstHorizontalSpeed)};
        });

    ParticleSystem::EmitterConfig ambient;
    ambient.texture = "white_x16.png";
    ambient.frequency = 0.08f;
    ambient.duration = std::numeric_limits<float>::max();
    ambient.spawnCount = 2;
    ambient.size = {0.24f, 0.24f, 0.24f};
    ambient.particleLifetime = 0.8f;
    ambient.spawnFuncKey = AMBIENT_SPAWN;
    ambient.colorKeys = {
        GradientKey<Vector4>{0.0f,
            {effectColor_.x, effectColor_.y, effectColor_.z, 0.75f}},
        GradientKey<Vector4>{1.0f,
            {effectColor_.x * 0.45f, effectColor_.y * 0.65f,
             effectColor_.z * 0.45f, 0.0f}}
    };
    ambient.sizeKeys = {
        GradientKey<Vector3>{0.0f, {0.24f, 0.24f, 0.24f}},
        GradientKey<Vector3>{1.0f, {0.04f, 0.04f, 0.04f}}
    };
    ParticleSystem::Template ambientTemplate;
    ambientTemplate.emitters.push_back(ambient);
    context_.particleSystem->Register(AMBIENT_TEMPLATE, ambientTemplate, true);

    ParticleSystem::EmitterConfig burst;
    burst.texture = "white_x16.png";
    burst.frequency = 0.0f;
    burst.duration = 0.0f;
    burst.spawnCount = completionBurstCount_;
    burst.size = {0.9f, 0.9f, 0.9f};
    burst.particleLifetime = 1.1f;
    burst.spawnFuncKey = COMPLETE_SPAWN;
    burst.colorKeys = {
        GradientKey<Vector4>{0.0f,
            {effectColor_.x, effectColor_.y, effectColor_.z, 1.0f}},
        GradientKey<Vector4>{1.0f,
            {effectColor_.x * 0.45f, effectColor_.y * 0.65f,
             effectColor_.z * 0.45f, 0.0f}}
    };
    burst.sizeKeys = {
        GradientKey<Vector3>{0.0f, {0.9f, 0.9f, 0.9f}},
        GradientKey<Vector3>{1.0f, {0.08f, 0.08f, 0.08f}}
    };
    ParticleSystem::Template burstTemplate;
    burstTemplate.emitters.push_back(burst);
    context_.particleSystem->Register(COMPLETE_TEMPLATE, burstTemplate, true);

    ambientEffect_ = context_.particleSystem->Emit(
        AMBIENT_TEMPLATE, targetTower_->GetPosition());
}

void TowerOrbitGimmick::UpdateOrbit(float _deltaTime) {
    const Vector3 offset = context_.player->GetPosition() - targetTower_->GetPosition();
    const bool connectedToTarget = context_.laser->GetConnectedTarget() == targetTower_;
    isPlayerOrbiting_ = false;

    if (connectedToTarget) {
        const float angle = std::atan2(offset.z, offset.x);
        if (hasPreviousAngle_) {
            const float delta = std::atan2(
                std::sin(angle - previousAngle_), std::cos(angle - previousAngle_));
            // 現在のカメラでは -Z が画面下側なので、角度の減少が右回り。
            const float directedDelta = direction_ == OrbitDirection::Clockwise
                ? -delta
                : delta;
            isPlayerOrbiting_ = directedDelta > 0.0001f;
            // 逆向きに回った分は相殺するが、0より下にはしない。
            accumulatedAngle_ = std::max(accumulatedAngle_ + directedDelta, 0.0f);
        }
        previousAngle_ = angle;
        hasPreviousAngle_ = true;
    } else {
        hasPreviousAngle_ = false;
    }

    const float arrowSpeedDegrees = isPlayerOrbiting_
        ? arrowActiveSpeedDegrees_
        : arrowIdleSpeedDegrees_;
    const float directionSign = direction_ == OrbitDirection::Clockwise ? -1.0f : 1.0f;
    arrowRotation_ = std::remainder(
        arrowRotation_ + directionSign * arrowSpeedDegrees
            * MathUtils::F_PI / 180.0f * _deltaTime,
        MathUtils::F_PI * 2.0f);
    UpdateArrowVisuals();

    const float requiredAngle = requiredDegrees_ * MathUtils::F_PI / 180.0f;
    const float progress = std::clamp(
        accumulatedAngle_ / requiredAngle, 0.0f, 1.0f);
    const float radiusScale = std::max(aoeRadius_ * progress, 0.01f);
    progressAoE_->SetScale({radiusScale, radiusScale, 1.0f});
    progressAoE_->SetColor({
        effectColor_.x, effectColor_.y, effectColor_.z, 0.72f});
    progressAoE_->Update();

    if (progress >= 1.0f) BeginCompletion();
}

void TowerOrbitGimmick::UpdateArrowVisuals(float _alpha) {
    if (!targetTower_) return;

    constexpr float QUARTER_TURN = MathUtils::F_PI * 0.5f;
    const float radius = aoeRadius_ * arrowRadiusRatio_;
    const float directionSign = direction_ == OrbitDirection::Clockwise ? -1.0f : 1.0f;
    const Vector3 center = targetTower_->GetPosition()
        + Vector3{0.0f, AOE_HEIGHT * 3.0f, 0.0f};

    for (std::size_t i = 0; i < arrows_.size(); ++i) {
        if (!arrows_[i]) continue;

        const float radialAngle = arrowRotation_ + QUARTER_TURN * static_cast<float>(i);
        const Vector3 position = center + Vector3{
            std::cos(radialAngle) * radius,
            0.0f,
            std::sin(radialAngle) * radius};

        // arrow.png は左向き。その -X 軸を周回方向の接線へ合わせる。
        const Vector3 tangent{
            std::sin(radialAngle) * directionSign,
            0.0f,
            -std::cos(radialAngle) * directionSign};
        const float yaw = std::atan2(tangent.z, -tangent.x);

        arrows_[i]->SetTranslate(position);
        arrows_[i]->SetRotate({-MathUtils::F_PI * 0.5f, yaw, 0.0f});
        arrows_[i]->SetColor({
            effectColor_.x, effectColor_.y, effectColor_.z, 0.95f * _alpha});
        arrows_[i]->Update();
    }
}

void TowerOrbitGimmick::BeginCompletion() {
    phase_ = Phase::Completion;
    completionElapsed_ = 0.0f;
    ambientEffect_.Stop();
    ambientEffect_ = {};

    // 達成したフレームでAoE全体を白くし、次フレーム以降で元の色へ戻す。
    baseAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    baseAoE_->Update();
    progressAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    progressAoE_->Update();

    if (context_.particleSystem) {
        context_.particleSystem->Emit(
            COMPLETE_TEMPLATE,
            targetTower_->GetPosition() + Vector3{0.0f, 0.2f, 0.0f});
    }
}

void TowerOrbitGimmick::UpdateCompletion(float _deltaTime) {
    completionElapsed_ = std::min(completionElapsed_ + _deltaTime, completionSeconds_);
    const float progress = std::clamp(
        completionElapsed_ / completionSeconds_, 0.0f, 1.0f);
    const float eased = EaseOutCubic(progress);
    const float scale = aoeRadius_ * (1.0f + (shockwaveScale_ - 1.0f) * eased);
    const float flash = 1.0f - std::clamp(
        completionElapsed_ / completionFlashSeconds_, 0.0f, 1.0f);

    const float directionSign = direction_ == OrbitDirection::Clockwise ? -1.0f : 1.0f;
    arrowRotation_ += directionSign * arrowActiveSpeedDegrees_
        * MathUtils::F_PI / 180.0f * _deltaTime;
    UpdateArrowVisuals(1.0f - progress);

    baseAoE_->SetColor({
        1.0f, 1.0f, 1.0f,
        (0.38f + 0.62f * flash) * (1.0f - progress)});
    baseAoE_->Update();
    progressAoE_->SetScale({scale, scale, 1.0f});
    progressAoE_->SetColor({
        effectColor_.x + (1.0f - effectColor_.x) * flash,
        effectColor_.y + (1.0f - effectColor_.y) * flash,
        effectColor_.z + (1.0f - effectColor_.z) * flash,
        (0.72f + 0.28f * flash) * (1.0f - progress)});
    progressAoE_->Update();

    if (completionElapsed_ >= completionSeconds_) state_ = GimmickState::Success;
}
