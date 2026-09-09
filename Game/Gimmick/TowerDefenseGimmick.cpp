#include "TowerDefenseGimmick.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

#include "Camera/Controller/CameraController.hpp"
#include "Enemy/EnemyManager.hpp"
#include "Json/JsonParams.hpp"
#include "Math/Easing.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Vector4.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
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
    constexpr Vector4 SPAWNER_TOWER_COLOR{1.0f, 0.1f, 0.1f, 1.0f};
    constexpr Vector4 WARNING_ARROW_COLOR{1.0f, 0.15f, 0.15f, 1.0f};
    constexpr float EDGE_PADDING = 12.0f;
    constexpr float EPSILON = 0.0001f;
    constexpr float AOE_HEIGHT = 0.04f;
    constexpr const char* COMPLETE_TEMPLATE = "TowerDefenseComplete";
    constexpr const char* COMPLETE_SPAWN = "TowerDefenseCompleteSpawn";
}

void TowerDefenseGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    phase_ = Phase::Warning;
    warningElapsedSeconds_ = 0.0f;
    warningArrowVisible_ = false;
    spawnElapsedSeconds_ = 0.0f;
    killCount_ = 0;

    LoadConfig();

    targetTower_ = context_.towerManager ? context_.towerManager->PickRandomIdleTower() : nullptr;
    if (!targetTower_) {
        // 対象にできるタワーが無ければ何もせず成功扱いで終える。
        state_ = GimmickState::Success;
        return;
    }

    targetTower_->SetColorOverride(SPAWNER_TOWER_COLOR);
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(true);
    if (context_.enemyManager) context_.enemyManager->SetSpawnSuspended(true);

    warningArrow_ = std::make_unique<Sprite>();
    warningArrow_->Initialize("arrow.png");
    warningArrow_->SetAnchorPoint({0.5f, 0.5f});
    warningArrow_->SetColor(WARNING_ARROW_COLOR);
    warningArrow_->SetSize(warningArrowSize_);

    InitializeAoEPlane();
    InitializeCompletionParticles();

    state_ = GimmickState::Active;
}

void TowerDefenseGimmick::Update(float _deltaTime) {
    if (state_ != GimmickState::Active) return;
    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    if (phase_ == Phase::Warning) {
        UpdateWarningArrow();
        warningElapsedSeconds_ += _deltaTime;
        if (warningElapsedSeconds_ >= warningDurationSeconds_) {
            phase_ = Phase::Spawning;
            warningArrowVisible_ = false;
        }
        return;
    }

    if (phase_ == Phase::Completion) {
        UpdateCompletion(_deltaTime);
        return;
    }

    if (context_.enemyManager && targetTower_) {
        spawnElapsedSeconds_ += _deltaTime;
        if (spawnElapsedSeconds_ >= spawnIntervalSeconds_) {
            spawnElapsedSeconds_ = std::fmod(spawnElapsedSeconds_, spawnIntervalSeconds_);
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const float radius = std::sqrt(MathUtils::Random(0.0f, 1.0f)) * killRadius_ * spawnRadiusRatio_;
            const Vector3 offset{std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
            context_.enemyManager->SpawnExtraEnemy(targetTower_->GetPosition() + offset);
        }

        CollectKillsInRange();
        UpdateProgressVisual();
    }

    if (killCount_ >= requiredKillCount_) {
        BeginCompletion();
    }
}

void TowerDefenseGimmick::CollectKillsInRange() {
    if (!targetTower_) return;
    const Vector3 towerPosition = targetTower_->GetPosition();
    for (const Vector3& position : context_.enemyManager->GetRecentDefeatPositions()) {
        if (MathUtils::SquaredDistance(position, towerPosition) <= killRadius_ * killRadius_) {
            ++killCount_;
        }
    }
}

void TowerDefenseGimmick::UpdateWarningArrow() {
    warningArrowVisible_ = false;
    if (!targetTower_ || !warningArrow_) return;

    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    const auto screen = Singleton<Screen>::GetInstance();
    const float width = screen->Width();
    const float height = screen->Height();
    const float screenMargin = std::max(warningArrowSize_.x, warningArrowSize_.y) * 0.5f + EDGE_PADDING;
    if (!camera || width <= screenMargin * 2.0f || height <= screenMargin * 2.0f) return;

    const Vector3 worldPosition = targetTower_->GetPosition();
    const Vector4 clip = MathUtils::Matrix::Transform(
        Vector4{worldPosition.x, worldPosition.y, worldPosition.z, 1.0f},
        camera->GetViewProjection());
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y)
        || !std::isfinite(clip.z) || !std::isfinite(clip.w)
        || std::abs(clip.w) <= EPSILON) return;

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float ndcZ = clip.z / clip.w;
    const bool inFront = clip.w > 0.0f;
    const bool onScreen = inFront && ndcZ >= 0.0f && ndcZ <= 1.0f
        && std::abs(ndcX) <= 1.0f && std::abs(ndcY) <= 1.0f;
    if (onScreen) return;

    Vector2 direction{ndcX, -ndcY};
    if (!inFront) direction = direction * -1.0f;
    const float length = std::hypot(direction.x, direction.y);
    if (length <= EPSILON) direction = {0.0f, -1.0f};
    else direction = direction * (1.0f / length);

    const Vector2 center{width * 0.5f, height * 0.5f};
    const Vector2 halfArea{center.x - screenMargin, center.y - screenMargin};
    const float scaleX = std::abs(direction.x) > EPSILON
        ? halfArea.x / std::abs(direction.x) : INFINITY;
    const float scaleY = std::abs(direction.y) > EPSILON
        ? halfArea.y / std::abs(direction.y) : INFINITY;
    const Vector2 position = center + direction * std::min(scaleX, scaleY);

    warningArrow_->SetPosition(position);
    warningArrow_->SetRotation(std::atan2(direction.y, direction.x) - MathUtils::F_PI);
    warningArrow_->Update();
    warningArrowVisible_ = true;
}

void TowerDefenseGimmick::Draw() const {
    if (baseAoE_) baseAoE_->Draw();
    if (progressAoE_) progressAoE_->Draw();
    if (warningArrowVisible_ && warningArrow_) warningArrow_->Draw();
}

void TowerDefenseGimmick::InitializeAoEPlane() {
    if (!targetTower_) return;

    const Vector3 center = targetTower_->GetPosition() + Vector3{0.0f, AOE_HEIGHT, 0.0f};
    baseAoE_ = std::make_unique<Model>();
    baseAoE_->Initialize("plane");
    baseAoE_->SetTexture("circle2.png");
    baseAoE_->SetTranslate(center);
    baseAoE_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    baseAoE_->SetScale({killRadius_, killRadius_, 1.0f});
    baseAoE_->SetColor({1.0f, 1.0f, 1.0f, 0.38f});
    baseAoE_->Update();

    progressAoE_ = std::make_unique<Model>();
    progressAoE_->Initialize("plane");
    progressAoE_->SetTexture("circle2.png");
    progressAoE_->SetTranslate(center + Vector3{0.0f, AOE_HEIGHT, 0.0f});
    progressAoE_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    progressAoE_->SetScale({0.01f, 0.01f, 1.0f});
    progressAoE_->SetColor({effectColor_.x, effectColor_.y, effectColor_.z, 0.72f});
    progressAoE_->Update();
}

void TowerDefenseGimmick::UpdateProgressVisual() {
    if (!progressAoE_) return;

    const float progress = std::clamp(
        static_cast<float>(killCount_) / static_cast<float>(requiredKillCount_), 0.0f, 1.0f);
    const float radiusScale = std::max(killRadius_ * progress, 0.01f);
    progressAoE_->SetScale({radiusScale, radiusScale, 1.0f});
    progressAoE_->Update();
}

void TowerDefenseGimmick::InitializeCompletionParticles() {
    if (!context_.particleSystem) return;

    const float effectRadius = killRadius_;
    context_.particleSystem->RegisterSpawnFunc(COMPLETE_SPAWN,
        [effectRadius](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const float radius = std::sqrt(MathUtils::Random(0.0f, 1.0f)) * effectRadius;
            _position = _center + Vector3{
                std::cos(angle) * radius,
                MathUtils::Random(0.05f, 0.45f),
                std::sin(angle) * radius};
            _velocity = {
                MathUtils::Random(-1.5f, 1.5f),
                MathUtils::Random(8.0f, 14.0f),
                MathUtils::Random(-1.5f, 1.5f)};
        });

    const Vector4 color = effectColor_;
    ParticleSystem::EmitterConfig burst;
    burst.texture = "white_x16.png";
    burst.frequency = 0.0f;
    burst.duration = 0.0f;
    burst.spawnCount = 64;
    burst.size = {0.9f, 0.9f, 0.9f};
    burst.particleLifetime = 1.1f;
    burst.spawnFuncKey = COMPLETE_SPAWN;
    burst.colorKeys = {
        GradientKey<Vector4>{0.0f, color},
        GradientKey<Vector4>{1.0f, {color.x * 0.45f, color.y * 0.65f, color.z * 0.45f, 0.0f}}
    };
    burst.sizeKeys = {
        GradientKey<Vector3>{0.0f, {0.9f, 0.9f, 0.9f}},
        GradientKey<Vector3>{1.0f, {0.08f, 0.08f, 0.08f}}
    };
    ParticleSystem::Template burstTemplate;
    burstTemplate.emitters.push_back(burst);
    context_.particleSystem->Register(COMPLETE_TEMPLATE, burstTemplate, true);
}

void TowerDefenseGimmick::BeginCompletion() {
    phase_ = Phase::Completion;
    completionElapsed_ = 0.0f;
    warningArrowVisible_ = false;

    if (baseAoE_) {
        baseAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        baseAoE_->Update();
    }
    if (progressAoE_) {
        progressAoE_->SetScale({killRadius_, killRadius_, 1.0f});
        progressAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        progressAoE_->Update();
    }

    if (context_.particleSystem && targetTower_) {
        context_.particleSystem->Emit(
            COMPLETE_TEMPLATE, targetTower_->GetPosition() + Vector3{0.0f, 0.2f, 0.0f});
    }
}

void TowerDefenseGimmick::UpdateCompletion(float _deltaTime) {
    completionElapsed_ = std::min(completionElapsed_ + _deltaTime, completionSeconds_);
    const float progress = std::clamp(completionElapsed_ / completionSeconds_, 0.0f, 1.0f);

    const Vector3 scale = Ease::Out::Cubic(
        Vector3{killRadius_, killRadius_, 1.0f},
        Vector3{killRadius_ * 1.4f, killRadius_ * 1.4f, 1.0f},
        progress);
    const float alpha = Ease::Out::Cubic(Vector2{1.0f, 0.0f}, Vector2{0.0f, 0.0f}, progress).x;

    if (baseAoE_) {
        baseAoE_->SetScale(scale);
        baseAoE_->SetColor({1.0f, 1.0f, 1.0f, alpha});
        baseAoE_->Update();
    }
    if (progressAoE_) {
        progressAoE_->SetScale(scale);
        progressAoE_->SetColor({1.0f, 1.0f, 1.0f, alpha});
        progressAoE_->Update();
    }

    if (completionElapsed_ >= completionSeconds_) Finish(GimmickState::Success);
}

void TowerDefenseGimmick::Debug() {
#ifdef _DEBUG
    ImGui::Begin("TowerDefenseGimmick");
    ImGui::Text("State: %s", state_ == GimmickState::Active ? "Active"
        : state_ == GimmickState::Success ? "Success"
        : state_ == GimmickState::Failed ? "Failed" : "Ready");
    ImGui::Text("Phase: %s", phase_ == Phase::Warning ? "Warning"
        : phase_ == Phase::Spawning ? "Spawning" : "Completion");
    ImGui::Text("Warning: %.2f / %.2f", warningElapsedSeconds_, warningDurationSeconds_);
    ImGui::Text("Kills: %d / %d", killCount_, requiredKillCount_);

    DebugUIWidgets::DragFloat("Warning Seconds", &warningDurationSeconds_, 0.1f, 0.0f, 20.0f);
    DebugUIWidgets::DragFloat("Time Limit", &timeLimitSeconds_, 0.1f, 5.0f, 120.0f);
    int32_t requiredKillCount = requiredKillCount_;
    if (ImGui::DragInt("Required Kill Count", &requiredKillCount, 1, 1, 999)) {
        requiredKillCount_ = requiredKillCount;
    }
    DebugUIWidgets::DragFloat("Kill Radius", &killRadius_, 0.1f, 1.0f, 50.0f);
    DebugUIWidgets::DragFloat("Spawn Radius Ratio", &spawnRadiusRatio_, 0.01f, 0.1f, 1.0f);
    DebugUIWidgets::DragFloat("Spawn Interval", &spawnIntervalSeconds_, 0.05f, 0.1f, 10.0f);
    DebugUIWidgets::DragFloat("Completion Seconds", &completionSeconds_, 0.05f, 0.1f, 5.0f);

    if (ImGui::Button("Save Tuning")) SaveConfig();
    ImGui::End();
#endif
}

void TowerDefenseGimmick::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "TowerDefense")) return;
    const auto groups = json->GetGroups("TowerDefense");
    const auto tuning = groups.find("Tuning");
    if (tuning == groups.end()) return;

    const auto readFloat = [](const auto& _group, const char* _key, float _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto value = std::get_if<float>(&entry->second)) return *value;
        if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*integerValue);
        }
        return _fallback;
    };
    const auto readInt = [](const auto& _group, const char* _key, int32_t _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto value = std::get_if<int32_t>(&entry->second)) return *value;
        if (const auto floatValue = std::get_if<float>(&entry->second)) {
            return static_cast<int32_t>(*floatValue);
        }
        return _fallback;
    };

    warningDurationSeconds_ = readFloat(tuning->second, "WarningSeconds", warningDurationSeconds_);
    timeLimitSeconds_ = readFloat(tuning->second, "TimeLimitSeconds", timeLimitSeconds_);
    requiredKillCount_ = readInt(tuning->second, "RequiredKillCount", requiredKillCount_);
    killRadius_ = readFloat(tuning->second, "KillRadius", killRadius_);
    spawnRadiusRatio_ = readFloat(tuning->second, "SpawnRadiusRatio", spawnRadiusRatio_);
    spawnIntervalSeconds_ = readFloat(tuning->second, "SpawnIntervalSeconds", spawnIntervalSeconds_);
    completionSeconds_ = readFloat(tuning->second, "CompletionSeconds", completionSeconds_);

    warningDurationSeconds_ = std::isfinite(warningDurationSeconds_)
        ? std::max(warningDurationSeconds_, 0.0f) : 3.0f;
    timeLimitSeconds_ = std::isfinite(timeLimitSeconds_) ? std::max(timeLimitSeconds_, 1.0f) : 25.0f;
    requiredKillCount_ = std::max(requiredKillCount_, 1);
    killRadius_ = std::isfinite(killRadius_) ? std::max(killRadius_, 0.1f) : 20.0f;
    spawnRadiusRatio_ = std::isfinite(spawnRadiusRatio_)
        ? std::clamp(spawnRadiusRatio_, 0.1f, 1.0f) : 0.8f;
    spawnIntervalSeconds_ = std::isfinite(spawnIntervalSeconds_)
        ? std::max(spawnIntervalSeconds_, 0.05f) : 1.5f;
    completionSeconds_ = std::isfinite(completionSeconds_)
        ? std::max(completionSeconds_, 0.05f) : 0.6f;
}

void TowerDefenseGimmick::SaveConfig() const {
    const auto json = Singleton<JsonParams>::GetInstance();
    json->SetValue("TowerDefense", "Tuning", "WarningSeconds", warningDurationSeconds_);
    json->SetValue("TowerDefense", "Tuning", "TimeLimitSeconds", timeLimitSeconds_);
    json->SetValue("TowerDefense", "Tuning", "RequiredKillCount", requiredKillCount_);
    json->SetValue("TowerDefense", "Tuning", "KillRadius", killRadius_);
    json->SetValue("TowerDefense", "Tuning", "SpawnRadiusRatio", spawnRadiusRatio_);
    json->SetValue("TowerDefense", "Tuning", "SpawnIntervalSeconds", spawnIntervalSeconds_);
    json->SetValue("TowerDefense", "Tuning", "CompletionSeconds", completionSeconds_);
    json->Save("Gimmick", "TowerDefense");
}

void TowerDefenseGimmick::Finish(GimmickState _result) {
    state_ = _result;
    warningArrowVisible_ = false;
    if (targetTower_) targetTower_->SetColorOverride(std::nullopt);
    targetTower_ = nullptr;
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(false);
    if (context_.enemyManager) context_.enemyManager->SetSpawnSuspended(false);
}
