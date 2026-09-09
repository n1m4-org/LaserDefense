#include "TowerDefenseGimmick.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

#include "Camera/Controller/CameraController.hpp"
#include "Enemy/EnemyManager.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Vector4.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Tower/MainTower.hpp"
#include "Tower/TowerManager.hpp"

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
}

void TowerDefenseGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    phase_ = Phase::Warning;
    warningElapsedSeconds_ = 0.0f;
    warningArrowVisible_ = false;
    elapsedTime_ = 0.0f;
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

    warningArrow_ = std::make_unique<Sprite>();
    warningArrow_->Initialize("arrow.png");
    warningArrow_->SetAnchorPoint({0.5f, 0.5f});
    warningArrow_->SetColor(WARNING_ARROW_COLOR);
    warningArrow_->SetSize(warningArrowSize_);

    state_ = GimmickState::Active;
    LoadConfig();
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

    elapsedTime_ += _deltaTime;

    if (context_.enemyManager && targetTower_) {
        spawnElapsedSeconds_ += _deltaTime;
        if (spawnElapsedSeconds_ >= spawnIntervalSeconds_) {
            spawnElapsedSeconds_ = std::fmod(spawnElapsedSeconds_, spawnIntervalSeconds_);
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const Vector3 offset{
                std::cos(angle) * spawnRadius_, 0.0f, std::sin(angle) * spawnRadius_};
            context_.enemyManager->SpawnExtraEnemy(targetTower_->GetPosition() + offset);
        }

        CollectKillsInRange();
    }

    if (killCount_ >= requiredKillCount_) {
        Finish(GimmickState::Success);
        return;
    }

    if (elapsedTime_ >= timeLimitSeconds_) {
        Finish(GimmickState::Failed);
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
    if (warningArrowVisible_ && warningArrow_) warningArrow_->Draw();
}

void TowerDefenseGimmick::Debug() {
#ifdef _DEBUG
    ImGui::Begin("TowerDefenseGimmick");
    ImGui::Text("State: %s", state_ == GimmickState::Active ? "Active"
        : state_ == GimmickState::Success ? "Success"
        : state_ == GimmickState::Failed ? "Failed" : "Ready");
    ImGui::Text("Phase: %s", phase_ == Phase::Warning ? "Warning" : "Spawning");
    ImGui::Text("Warning: %.2f / %.2f", warningElapsedSeconds_, warningDurationSeconds_);
    ImGui::Text("Elapsed: %.2f / %.2f", elapsedTime_, timeLimitSeconds_);
    ImGui::Text("Kills: %d / %d", killCount_, requiredKillCount_);

    DebugUIWidgets::DragFloat("Warning Seconds", &warningDurationSeconds_, 0.1f, 0.0f, 20.0f);
    DebugUIWidgets::DragFloat("Time Limit", &timeLimitSeconds_, 0.1f, 5.0f, 120.0f);
    int32_t requiredKillCount = requiredKillCount_;
    if (ImGui::DragInt("Required Kill Count", &requiredKillCount, 1, 1, 999)) {
        requiredKillCount_ = requiredKillCount;
    }
    DebugUIWidgets::DragFloat("Kill Radius", &killRadius_, 0.1f, 1.0f, 50.0f);
    DebugUIWidgets::DragFloat("Spawn Interval", &spawnIntervalSeconds_, 0.05f, 0.1f, 10.0f);
    DebugUIWidgets::DragFloat("Spawn Radius", &spawnRadius_, 0.1f, 0.0f, 20.0f);

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
    spawnIntervalSeconds_ = readFloat(tuning->second, "SpawnIntervalSeconds", spawnIntervalSeconds_);
    spawnRadius_ = readFloat(tuning->second, "SpawnRadius", spawnRadius_);

    warningDurationSeconds_ = std::isfinite(warningDurationSeconds_)
        ? std::max(warningDurationSeconds_, 0.0f) : 3.0f;
    timeLimitSeconds_ = std::isfinite(timeLimitSeconds_) ? std::max(timeLimitSeconds_, 1.0f) : 25.0f;
    requiredKillCount_ = std::max(requiredKillCount_, 1);
    killRadius_ = std::isfinite(killRadius_) ? std::max(killRadius_, 0.1f) : 8.0f;
    spawnIntervalSeconds_ = std::isfinite(spawnIntervalSeconds_)
        ? std::max(spawnIntervalSeconds_, 0.05f) : 1.5f;
    spawnRadius_ = std::isfinite(spawnRadius_) ? std::max(spawnRadius_, 0.0f) : 3.0f;
}

void TowerDefenseGimmick::SaveConfig() const {
    const auto json = Singleton<JsonParams>::GetInstance();
    json->SetValue("TowerDefense", "Tuning", "WarningSeconds", warningDurationSeconds_);
    json->SetValue("TowerDefense", "Tuning", "TimeLimitSeconds", timeLimitSeconds_);
    json->SetValue("TowerDefense", "Tuning", "RequiredKillCount", requiredKillCount_);
    json->SetValue("TowerDefense", "Tuning", "KillRadius", killRadius_);
    json->SetValue("TowerDefense", "Tuning", "SpawnIntervalSeconds", spawnIntervalSeconds_);
    json->SetValue("TowerDefense", "Tuning", "SpawnRadius", spawnRadius_);
    json->Save("Gimmick", "TowerDefense");
}

void TowerDefenseGimmick::Finish(GimmickState _result) {
    state_ = _result;
    warningArrowVisible_ = false;
    if (targetTower_) targetTower_->SetColorOverride(std::nullopt);
    targetTower_ = nullptr;
    if (context_.towerManager) context_.towerManager->SetMainTowerSwitchSuspended(false);
}
