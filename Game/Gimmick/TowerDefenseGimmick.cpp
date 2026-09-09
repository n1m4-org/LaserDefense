#include "TowerDefenseGimmick.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <variant>

#include "Enemy/EnemyManager.hpp"
#include "Sound/GameSound.hpp"
#include "Json/JsonParams.hpp"
#include "Math/Easing.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
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
    constexpr Vector4 SPAWNER_TOWER_COLOR{1.0f, 0.1f, 0.1f, 1.0f};
    constexpr float AOE_HEIGHT = 0.04f;
    constexpr float AOE_VISUAL_SCALE = 2.0f;
    constexpr const char* COMPLETE_TEMPLATE = "TowerDefenseComplete";
    constexpr const char* COMPLETE_SPAWN = "TowerDefenseCompleteSpawn";
}

void TowerDefenseGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    phase_ = Phase::Warning;
    warningElapsedSeconds_ = 0.0f;
    spawnElapsedSeconds_ = 0.0f;
    killCount_ = 0;
    spawnerEnemyRotation_ = 0.0f;
    absorptionCubes_.clear();
    fragments_.clear();
    absorptionPulseElapsed_ = 0.0f;
    absorptionPulseActive_ = false;

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

    InitializeSpawnerEnemyVisual();
    InitializeAoEPlane();
    InitializeCompletionParticles();

    // 予告の矢印が出るのと同時に鳴らして、画面外で始まっても気づけるようにする
    GameSound::Play(GameSound::Se::GimmickEnemySpawn);

    state_ = GimmickState::Active;
}

void TowerDefenseGimmick::Update(float _deltaTime) {
    if (state_ != GimmickState::Active) return;
    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    if (phase_ == Phase::Warning) {
        warningElapsedSeconds_ += _deltaTime;
        UpdateSpawnerEnemyVisual(_deltaTime);
        if (warningElapsedSeconds_ >= warningDurationSeconds_) {
            phase_ = Phase::Spawning;
        }
        return;
    }

    if (phase_ == Phase::Completion) {
        UpdateCompletionFragments(_deltaTime);
        UpdateCompletion(_deltaTime);
        return;
    }

    UpdateSpawnerEnemyVisual(_deltaTime);
    UpdateAbsorptionCubes(_deltaTime);
    UpdateAbsorptionPulse(_deltaTime);

    if (context_.enemyManager && targetTower_) {
        spawnElapsedSeconds_ += _deltaTime;
        if (spawnElapsedSeconds_ >= spawnIntervalSeconds_) {
            spawnElapsedSeconds_ = std::fmod(spawnElapsedSeconds_, spawnIntervalSeconds_);
            for (int32_t i = 0; i < spawnCount_; ++i) {
                const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
                const float radius = std::sqrt(MathUtils::Random(0.0f, 1.0f))
                    * killRadius_ * spawnRadiusRatio_;
                const Vector3 offset{std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
                context_.enemyManager->SpawnExtraEnemy(targetTower_->GetPosition() + offset);
            }
        }

        CollectKillsInRange();
        UpdateProgressVisual();
    }

    if (killCount_ >= requiredKillCount_
        && absorptionCubes_.empty() && !absorptionPulseActive_) {
        BeginCompletion();
    }
}

void TowerDefenseGimmick::CollectKillsInRange() {
    if (!targetTower_) return;
    const Vector3 towerPosition = targetTower_->GetPosition();
    for (const Vector3& position : context_.enemyManager->GetRecentDefeatPositions()) {
        if (MathUtils::SquaredDistance(position, towerPosition) <= killRadius_ * killRadius_) {
            ++killCount_;
            SpawnAbsorptionCube(position);
        }
    }
}

void TowerDefenseGimmick::InitializeSpawnerEnemyVisual() {
    if (!targetTower_) return;

    spawnerEnemyModel_ = std::make_unique<Model>();
    const std::string modelName = context_.enemyManager
        ? context_.enemyManager->GetModelName() : "Cube";
    spawnerEnemyModel_->Initialize(modelName);
    spawnerEnemyModel_->SetEnvironmentTexture("skybox.dds");
    const Vector3 enemyScale = context_.enemyManager
        ? context_.enemyManager->GetModelScale() : Vector3{0.5f, 0.5f, 0.5f};
    spawnerEnemyBaseScale_ = enemyScale * spawnerEnemyScaleMultiplier_;
    spawnerEnemyBaseColor_ = context_.enemyManager
        ? context_.enemyManager->GetModelColor() : Vector4{1.0f, 0.0f, 0.0f, 1.0f};
    spawnerEnemyModel_->SetScale(spawnerEnemyBaseScale_);
    spawnerEnemyModel_->SetColor(spawnerEnemyBaseColor_);
    spawnerEnemyModel_->SetTranslate(targetTower_->GetPosition()
        + Vector3{0.0f, spawnerEnemyStartHeight_, 0.0f});
    spawnerEnemyModel_->Update();
}

void TowerDefenseGimmick::UpdateSpawnerEnemyVisual(float _deltaTime) {
    if (!spawnerEnemyModel_ || !targetTower_) return;

    const Vector3 towerPosition = targetTower_->GetPosition();
    if (phase_ == Phase::Warning) {
        const float progress = warningDurationSeconds_ > 0.0f
            ? std::clamp(warningElapsedSeconds_ / warningDurationSeconds_, 0.0f, 1.0f)
            : 1.0f;
        const Vector3 start = towerPosition + Vector3{0.0f, spawnerEnemyStartHeight_, 0.0f};
        const Vector3 end = towerPosition + Vector3{0.0f, spawnerEnemyFloatHeight_, 0.0f};
        spawnerEnemyModel_->SetTranslate(Ease::Out::Cubic(start, end, progress));
    } else {
        spawnerEnemyRotation_ = std::fmod(
            spawnerEnemyRotation_ + spawnerEnemyRotationSpeed_ * _deltaTime,
            MathUtils::F_PI * 2.0f);
        spawnerEnemyModel_->SetTranslate(towerPosition
            + Vector3{0.0f, spawnerEnemyFloatHeight_, 0.0f});
        spawnerEnemyModel_->SetRotate({0.0f, spawnerEnemyRotation_, 0.0f});
    }
    spawnerEnemyModel_->Update();
}

void TowerDefenseGimmick::SpawnAbsorptionCube(const Vector3& _position) {
    AbsorptionCubeVisual visual;
    visual.model = std::make_unique<Model>();
    visual.model->Initialize("Cube");
    visual.model->SetEnvironmentTexture("skybox.dds");
    visual.model->SetScale(spawnerEnemyBaseScale_ * absorptionCubeScaleRatio_);
    visual.model->SetColor(spawnerEnemyBaseColor_);
    visual.startPosition = _position + Vector3{0.0f, 0.5f, 0.0f};
    visual.model->SetTranslate(visual.startPosition);
    visual.model->Update();
    absorptionCubes_.push_back(std::move(visual));
}

void TowerDefenseGimmick::UpdateAbsorptionCubes(float _deltaTime) {
    if (!targetTower_) return;

    const Vector3 targetPosition = targetTower_->GetPosition()
        + Vector3{0.0f, spawnerEnemyFloatHeight_, 0.0f};
    for (auto it = absorptionCubes_.begin(); it != absorptionCubes_.end();) {
        it->elapsedSeconds = std::min(it->elapsedSeconds + _deltaTime, absorptionSeconds_);
        const float progress = absorptionSeconds_ > 0.0f
            ? std::clamp(it->elapsedSeconds / absorptionSeconds_, 0.0f, 1.0f)
            : 1.0f;
        Vector3 position = Ease::In::Cubic(it->startPosition, targetPosition, progress);
        position.y += std::sin(progress * MathUtils::F_PI) * absorptionArcHeight_;
        it->model->SetTranslate(position);
        it->model->SetRotate({
            progress * MathUtils::F_PI * 2.0f,
            progress * MathUtils::F_PI * 3.0f,
            progress * MathUtils::F_PI});
        it->model->SetScale(spawnerEnemyBaseScale_
            * (absorptionCubeScaleRatio_ * (1.0f - progress * 0.7f)));
        it->model->Update();

        if (progress >= 1.0f) {
            absorptionPulseElapsed_ = 0.0f;
            absorptionPulseActive_ = true;
            it = absorptionCubes_.erase(it);
        } else {
            ++it;
        }
    }
}

void TowerDefenseGimmick::UpdateAbsorptionPulse(float _deltaTime) {
    if (!absorptionPulseActive_ || !spawnerEnemyModel_) return;

    absorptionPulseElapsed_ = std::min(
        absorptionPulseElapsed_ + _deltaTime, absorptionPulseSeconds_);
    const float progress = absorptionPulseSeconds_ > 0.0f
        ? std::clamp(absorptionPulseElapsed_ / absorptionPulseSeconds_, 0.0f, 1.0f)
        : 1.0f;
    const float scalePulse = std::sin(progress * MathUtils::F_PI) * 0.1f;
    const float flash = std::max(1.0f - progress * 3.0f, 0.0f);
    spawnerEnemyModel_->SetScale(spawnerEnemyBaseScale_ * (1.0f + scalePulse));
    spawnerEnemyModel_->SetColor({
        spawnerEnemyBaseColor_.x + (1.0f - spawnerEnemyBaseColor_.x) * flash,
        spawnerEnemyBaseColor_.y + (1.0f - spawnerEnemyBaseColor_.y) * flash,
        spawnerEnemyBaseColor_.z + (1.0f - spawnerEnemyBaseColor_.z) * flash,
        spawnerEnemyBaseColor_.w});
    spawnerEnemyModel_->Update();

    if (progress >= 1.0f) {
        absorptionPulseActive_ = false;
        spawnerEnemyModel_->SetScale(spawnerEnemyBaseScale_);
        spawnerEnemyModel_->SetColor(spawnerEnemyBaseColor_);
        spawnerEnemyModel_->Update();
    }
}

void TowerDefenseGimmick::InitializeCompletionFragments() {
    if (!targetTower_) return;

    const Vector3 center = targetTower_->GetPosition()
        + Vector3{0.0f, spawnerEnemyFloatHeight_, 0.0f};
    constexpr std::array<Vector3, 4> DIRECTIONS{
        Vector3{-1.0f, 0.0f, -1.0f},
        Vector3{1.0f, 0.0f, -1.0f},
        Vector3{-1.0f, 0.0f, 1.0f},
        Vector3{1.0f, 0.0f, 1.0f},
    };

    fragments_.clear();
    fragments_.reserve(DIRECTIONS.size());
    for (std::size_t i = 0; i < DIRECTIONS.size(); ++i) {
        const Vector3& direction = DIRECTIONS[i];
        FragmentVisual fragment;
        fragment.model = std::make_unique<Model>();
        fragment.model->Initialize("Cube");
        fragment.model->SetEnvironmentTexture("skybox.dds");
        fragment.model->SetScale(spawnerEnemyBaseScale_ * fragmentScaleRatio_);
        fragment.model->SetColor(spawnerEnemyBaseColor_);
        fragment.position = center + Vector3{
            direction.x * spawnerEnemyBaseScale_.x * 0.45f,
            (i < 2 ? -1.0f : 1.0f) * spawnerEnemyBaseScale_.y * 0.25f,
            direction.z * spawnerEnemyBaseScale_.z * 0.45f};
        fragment.velocity = {
            direction.x * 1.6f,
            0.4f + static_cast<float>(i) * 0.12f,
            direction.z * 1.6f};
        fragment.angularVelocity = {
            1.8f + static_cast<float>(i) * 0.3f,
            direction.x * 2.2f,
            direction.z * 2.0f};
        fragment.model->SetTranslate(fragment.position);
        fragment.model->Update();
        fragments_.push_back(std::move(fragment));
    }
}

void TowerDefenseGimmick::UpdateCompletionFragments(float _deltaTime) {
    const float fade = completionSeconds_ > 0.0f
        ? 1.0f - std::clamp(completionElapsed_ / completionSeconds_, 0.0f, 1.0f)
        : 0.0f;
    for (FragmentVisual& fragment : fragments_) {
        fragment.velocity.y -= fragmentGravity_ * _deltaTime;
        fragment.position += fragment.velocity * _deltaTime;
        fragment.rotation += fragment.angularVelocity * _deltaTime;
        fragment.model->SetTranslate(fragment.position);
        fragment.model->SetRotate(fragment.rotation);
        fragment.model->SetColor({
            spawnerEnemyBaseColor_.x,
            spawnerEnemyBaseColor_.y,
            spawnerEnemyBaseColor_.z,
            spawnerEnemyBaseColor_.w * fade});
        fragment.model->Update();
    }
}

void TowerDefenseGimmick::Draw() const {
    if (baseAoE_) baseAoE_->Draw();
    if (progressAoE_) progressAoE_->Draw();
    for (const AbsorptionCubeVisual& visual : absorptionCubes_) visual.model->Draw();
    if (spawnerEnemyModel_) spawnerEnemyModel_->Draw();
    for (const FragmentVisual& fragment : fragments_) fragment.model->Draw();
}

bool TowerDefenseGimmick::GetIndicatorPosition(Vector3& _position) const {
    if (state_ != GimmickState::Active || !targetTower_ || !targetTower_->IsActive()) {
        return false;
    }
    _position = targetTower_->GetPosition();
    return true;
}

void TowerDefenseGimmick::InitializeAoEPlane() {
    if (!targetTower_) return;

    const Vector3 center = targetTower_->GetPosition() + Vector3{0.0f, AOE_HEIGHT, 0.0f};
    baseAoE_ = std::make_unique<Model>();
    baseAoE_->Initialize("plane");
    baseAoE_->SetTexture("circle2.png");
    baseAoE_->SetTranslate(center);
    baseAoE_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    const float visualRadius = killRadius_ * AOE_VISUAL_SCALE;
    baseAoE_->SetScale({visualRadius, visualRadius, 1.0f});
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
    const float radiusScale = std::max(killRadius_ * AOE_VISUAL_SCALE * progress, 0.01f);
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

    if (baseAoE_) {
        baseAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        baseAoE_->Update();
    }
    if (progressAoE_) {
        const float visualRadius = killRadius_ * AOE_VISUAL_SCALE;
        progressAoE_->SetScale({visualRadius, visualRadius, 1.0f});
        progressAoE_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        progressAoE_->Update();
    }

    InitializeCompletionFragments();
    spawnerEnemyModel_.reset();

    if (context_.particleSystem && targetTower_) {
        context_.particleSystem->Emit(
            COMPLETE_TEMPLATE, targetTower_->GetPosition() + Vector3{0.0f, 0.2f, 0.0f});
    }
}

void TowerDefenseGimmick::UpdateCompletion(float _deltaTime) {
    completionElapsed_ = std::min(completionElapsed_ + _deltaTime, completionSeconds_);
    const float progress = std::clamp(completionElapsed_ / completionSeconds_, 0.0f, 1.0f);

    const float visualRadius = killRadius_ * AOE_VISUAL_SCALE;
    const Vector3 scale = Ease::Out::Cubic(
        Vector3{visualRadius, visualRadius, 1.0f},
        Vector3{visualRadius * 1.4f, visualRadius * 1.4f, 1.0f},
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
    DebugUIWidgets::DragFloat("Enemy Scale Multiplier", &spawnerEnemyScaleMultiplier_, 0.1f, 0.1f, 10.0f);
    DebugUIWidgets::DragFloat("Enemy Start Height", &spawnerEnemyStartHeight_, 0.1f, -10.0f, 20.0f);
    DebugUIWidgets::DragFloat("Enemy Float Height", &spawnerEnemyFloatHeight_, 0.1f, -10.0f, 20.0f);
    DebugUIWidgets::DragFloat("Enemy Rotation Speed", &spawnerEnemyRotationSpeed_, 0.05f, 0.0f, 10.0f);
    DebugUIWidgets::DragFloat("Absorption Seconds", &absorptionSeconds_, 0.05f, 0.05f, 3.0f);
    DebugUIWidgets::DragFloat("Absorption Arc Height", &absorptionArcHeight_, 0.1f, 0.0f, 20.0f);
    DebugUIWidgets::DragFloat("Absorption Cube Scale", &absorptionCubeScaleRatio_, 0.01f, 0.01f, 1.0f);
    DebugUIWidgets::DragFloat("Absorption Pulse Seconds", &absorptionPulseSeconds_, 0.05f, 0.05f, 2.0f);
    DebugUIWidgets::DragFloat("Fragment Scale", &fragmentScaleRatio_, 0.01f, 0.05f, 1.0f);
    DebugUIWidgets::DragFloat("Fragment Gravity", &fragmentGravity_, 0.1f, 0.0f, 30.0f);
    DebugUIWidgets::DragFloat("Time Limit", &timeLimitSeconds_, 0.1f, 5.0f, 120.0f);
    int32_t requiredKillCount = requiredKillCount_;
    if (ImGui::DragInt("Required Kill Count", &requiredKillCount, 1, 1, 999)) {
        requiredKillCount_ = requiredKillCount;
    }
    DebugUIWidgets::DragFloat("Kill Radius", &killRadius_, 0.1f, 1.0f, 50.0f);
    DebugUIWidgets::DragFloat("Spawn Radius Ratio", &spawnRadiusRatio_, 0.01f, 0.1f, 1.0f);
    DebugUIWidgets::DragFloat("Spawn Interval", &spawnIntervalSeconds_, 0.05f, 0.1f, 10.0f);
    int32_t spawnCount = spawnCount_;
    if (ImGui::DragInt("Spawn Count", &spawnCount, 1, 1, 100)) {
        spawnCount_ = spawnCount;
    }
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
    spawnerEnemyScaleMultiplier_ = readFloat(
        tuning->second, "EnemyScaleMultiplier", spawnerEnemyScaleMultiplier_);
    spawnerEnemyStartHeight_ = readFloat(tuning->second, "EnemyStartHeight", spawnerEnemyStartHeight_);
    spawnerEnemyFloatHeight_ = readFloat(tuning->second, "EnemyFloatHeight", spawnerEnemyFloatHeight_);
    spawnerEnemyRotationSpeed_ = readFloat(tuning->second, "EnemyRotationSpeed", spawnerEnemyRotationSpeed_);
    absorptionSeconds_ = readFloat(tuning->second, "AbsorptionSeconds", absorptionSeconds_);
    absorptionArcHeight_ = readFloat(tuning->second, "AbsorptionArcHeight", absorptionArcHeight_);
    absorptionCubeScaleRatio_ = readFloat(
        tuning->second, "AbsorptionCubeScaleRatio", absorptionCubeScaleRatio_);
    absorptionPulseSeconds_ = readFloat(
        tuning->second, "AbsorptionPulseSeconds", absorptionPulseSeconds_);
    fragmentScaleRatio_ = readFloat(tuning->second, "FragmentScaleRatio", fragmentScaleRatio_);
    fragmentGravity_ = readFloat(tuning->second, "FragmentGravity", fragmentGravity_);
    timeLimitSeconds_ = readFloat(tuning->second, "TimeLimitSeconds", timeLimitSeconds_);
    requiredKillCount_ = readInt(tuning->second, "RequiredKillCount", requiredKillCount_);
    killRadius_ = readFloat(tuning->second, "KillRadius", killRadius_);
    spawnRadiusRatio_ = readFloat(tuning->second, "SpawnRadiusRatio", spawnRadiusRatio_);
    spawnIntervalSeconds_ = readFloat(tuning->second, "SpawnIntervalSeconds", spawnIntervalSeconds_);
    spawnCount_ = readInt(tuning->second, "SpawnCount", spawnCount_);
    completionSeconds_ = readFloat(tuning->second, "CompletionSeconds", completionSeconds_);

    warningDurationSeconds_ = std::isfinite(warningDurationSeconds_)
        ? std::max(warningDurationSeconds_, 0.0f) : 1.0f;
    spawnerEnemyScaleMultiplier_ = std::isfinite(spawnerEnemyScaleMultiplier_)
        ? std::max(spawnerEnemyScaleMultiplier_, 0.1f) : 4.0f;
    spawnerEnemyStartHeight_ = std::isfinite(spawnerEnemyStartHeight_) ? spawnerEnemyStartHeight_ : -0.5f;
    spawnerEnemyFloatHeight_ = std::isfinite(spawnerEnemyFloatHeight_) ? spawnerEnemyFloatHeight_ : 5.0f;
    spawnerEnemyRotationSpeed_ = std::isfinite(spawnerEnemyRotationSpeed_)
        ? std::max(spawnerEnemyRotationSpeed_, 0.0f) : 0.6f;
    absorptionSeconds_ = std::isfinite(absorptionSeconds_)
        ? std::max(absorptionSeconds_, 0.01f) : 0.45f;
    absorptionArcHeight_ = std::isfinite(absorptionArcHeight_)
        ? std::max(absorptionArcHeight_, 0.0f) : 2.0f;
    absorptionCubeScaleRatio_ = std::isfinite(absorptionCubeScaleRatio_)
        ? std::max(absorptionCubeScaleRatio_, 0.01f) : 0.125f;
    absorptionPulseSeconds_ = std::isfinite(absorptionPulseSeconds_)
        ? std::max(absorptionPulseSeconds_, 0.01f) : 0.25f;
    fragmentScaleRatio_ = std::isfinite(fragmentScaleRatio_)
        ? std::max(fragmentScaleRatio_, 0.01f) : 0.5f;
    fragmentGravity_ = std::isfinite(fragmentGravity_)
        ? std::max(fragmentGravity_, 0.0f) : 9.8f;
    timeLimitSeconds_ = std::isfinite(timeLimitSeconds_) ? std::max(timeLimitSeconds_, 1.0f) : 25.0f;
    requiredKillCount_ = std::max(requiredKillCount_, 1);
    killRadius_ = std::isfinite(killRadius_) ? std::max(killRadius_, 0.1f) : 20.0f;
    spawnRadiusRatio_ = std::isfinite(spawnRadiusRatio_)
        ? std::clamp(spawnRadiusRatio_, 0.1f, 1.0f) : 0.8f;
    spawnIntervalSeconds_ = std::isfinite(spawnIntervalSeconds_)
        ? std::max(spawnIntervalSeconds_, 0.05f) : 1.5f;
    spawnCount_ = std::max(spawnCount_, 1);
    completionSeconds_ = std::isfinite(completionSeconds_)
        ? std::max(completionSeconds_, 0.05f) : 0.6f;
}

void TowerDefenseGimmick::SaveConfig() const {
    const auto json = Singleton<JsonParams>::GetInstance();
    json->SetValue("TowerDefense", "Tuning", "WarningSeconds", warningDurationSeconds_);
    json->SetValue("TowerDefense", "Tuning", "EnemyScaleMultiplier", spawnerEnemyScaleMultiplier_);
    json->SetValue("TowerDefense", "Tuning", "EnemyStartHeight", spawnerEnemyStartHeight_);
    json->SetValue("TowerDefense", "Tuning", "EnemyFloatHeight", spawnerEnemyFloatHeight_);
    json->SetValue("TowerDefense", "Tuning", "EnemyRotationSpeed", spawnerEnemyRotationSpeed_);
    json->SetValue("TowerDefense", "Tuning", "AbsorptionSeconds", absorptionSeconds_);
    json->SetValue("TowerDefense", "Tuning", "AbsorptionArcHeight", absorptionArcHeight_);
    json->SetValue("TowerDefense", "Tuning", "AbsorptionCubeScaleRatio", absorptionCubeScaleRatio_);
    json->SetValue("TowerDefense", "Tuning", "AbsorptionPulseSeconds", absorptionPulseSeconds_);
    json->SetValue("TowerDefense", "Tuning", "FragmentScaleRatio", fragmentScaleRatio_);
    json->SetValue("TowerDefense", "Tuning", "FragmentGravity", fragmentGravity_);
    json->SetValue("TowerDefense", "Tuning", "TimeLimitSeconds", timeLimitSeconds_);
    json->SetValue("TowerDefense", "Tuning", "RequiredKillCount", requiredKillCount_);
    json->SetValue("TowerDefense", "Tuning", "KillRadius", killRadius_);
    json->SetValue("TowerDefense", "Tuning", "SpawnRadiusRatio", spawnRadiusRatio_);
    json->SetValue("TowerDefense", "Tuning", "SpawnIntervalSeconds", spawnIntervalSeconds_);
    json->SetValue("TowerDefense", "Tuning", "SpawnCount", spawnCount_);
    json->SetValue("TowerDefense", "Tuning", "CompletionSeconds", completionSeconds_);
    json->Save("Gimmick", "TowerDefense");
}

void TowerDefenseGimmick::Finish(GimmickState _result) {
    state_ = _result;
    spawnerEnemyModel_.reset();
    absorptionCubes_.clear();
    fragments_.clear();
    if (targetTower_) targetTower_->SetColorOverride(std::nullopt);
    targetTower_ = nullptr;
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(false);
    if (context_.enemyManager) context_.enemyManager->SetSpawnSuspended(false);
}
