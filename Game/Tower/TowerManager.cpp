#define NOMINMAX
#include "TowerManager.hpp"
#include "MainTower.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <variant>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    constexpr float SELECTION_SCALE = 1.5f;

    bool RayIntersectsAabb(const Vector3& _origin, const Vector3& _direction,
        const Vector3& _center, const Vector3& _size, float _length, float& _distance) {
        const Vector3 half = _size * (SELECTION_SCALE * 0.5f);
        const Vector3 minimum = _center - half;
        const Vector3 maximum = _center + half;
        float nearDistance = 0.0f;
        float farDistance = _length;

        const auto testAxis = [&](float _originValue, float _directionValue,
            float _minimum, float _maximum) {
            if (std::abs(_directionValue) <= 0.000001f) {
                return _originValue >= _minimum && _originValue <= _maximum;
            }
            float first = (_minimum - _originValue) / _directionValue;
            float second = (_maximum - _originValue) / _directionValue;
            if (first > second) std::swap(first, second);
            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);
            return nearDistance <= farDistance;
        };

        if (!testAxis(_origin.x, _direction.x, minimum.x, maximum.x)
            || !testAxis(_origin.y, _direction.y, minimum.y, maximum.y)
            || !testAxis(_origin.z, _direction.z, minimum.z, maximum.z)) return false;
        _distance = nearDistance;
        return nearDistance <= _length && farDistance >= 0.0f;
    }
}

Tower* TowerManager::PickTower(const Vector3& _origin, const Vector3& _direction, float _length) const {
    Tower* closestTower = nullptr;
    float closestDistance = std::numeric_limits<float>::max();
    for (const auto& tower : towers_) {
        if (!tower->IsActive()) continue;
        float distance = 0.0f;
        if (RayIntersectsAabb(_origin, _direction, tower->GetSelectionCenter(),
            tower->GetSelectionSize(), _length, distance) && distance < closestDistance) {
            closestDistance = distance;
            closestTower = tower.get();
        }
    }
    return closestTower;
}

void TowerManager::SetHoveredTower(const Tower* _tower) {
    for (const auto& tower : towers_) {
        tower->SetHovered(tower.get() == _tower);
    }
}

void TowerManager::SetConnectedTower(const Tower* _tower) {
    for (const auto& tower : towers_) {
        tower->SetConnected(tower.get() == _tower);
    }
}

void TowerManager::Initialize() {
    towers_.clear();
    towerCandidates_.clear();
    mainTowers_.clear();
    mainTowerSwitchTime_ = 0.0f;
    nextCandidateIndex_ = 0;
    mainTowerSwitched_ = false;
    mainTowerSwitchSuspended_ = false;
    switchWarningTower_ = nullptr;
    nextWarningTower_ = nullptr;
    LoadConfig();
    ResetHp();
}

Tower* TowerManager::AddTower(const Vector3& _position) {
    // すべての設置枠を将来メイン化できる候補として生成する。
    auto tower = std::make_unique<MainTower>();
    tower->Initialize();
    tower->SetDefenseTarget(false, false);
    tower->SetPosition(_position);
    Tower* addedTower = tower.get();
    towerCandidates_.push_back(tower.get());
    towers_.push_back(std::move(tower));
    return addedTower;
}

MainTower* TowerManager::AddMainTower(const Vector3& _position) {
    auto tower = std::make_unique<MainTower>();
    tower->Initialize();
    tower->SetPosition(_position);
    tower->Update(0.0f);
    MainTower* addedTower = tower.get();
    towerCandidates_.push_back(addedTower);
    mainTowers_.push_back(addedTower);
    towers_.push_back(std::move(tower));
    return addedTower;
}

void TowerManager::Update(float _deltaTime) {
    UpdateMainTowerSwitch(_deltaTime);
    for (const auto& tower : towers_) {
        if (tower->IsActive()) {
            tower->Update(_deltaTime);
        }
    }
}

void TowerManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Tower", "MainTower")) return;
    const auto groups = json->GetGroups("MainTower");
    if (const auto health = groups.find("Health"); health != groups.end()) {
        if (const auto entry = health->second.find("MaxHp"); entry != health->second.end()) {
            if (const auto value = std::get_if<float>(&entry->second)) maxHp_ = *value;
            else if (const auto valueInt = std::get_if<int32_t>(&entry->second)) maxHp_ = static_cast<float>(*valueInt);
        }
    }
    maxHp_ = std::isfinite(maxHp_) ? std::max(maxHp_, 1.0f) : 100.0f;
    const auto group = groups.find("Switch");
    if (group == groups.end()) return;
    const auto readNumber = [&](const char* _key, float _fallback) {
        const auto entry = group->second.find(_key);
        if (entry == group->second.end()) return _fallback;
        if (const auto value = std::get_if<float>(&entry->second)) return *value;
        if (const auto value = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*value);
        }
        return _fallback;
    };
    mainTowerSwitchInterval_ = readNumber("IntervalSeconds", mainTowerSwitchInterval_);
    switchWarningLeadSeconds_ = readNumber(
        "WarningLeadSeconds", switchWarningLeadSeconds_);
    switchWarningBlinkInterval_ = readNumber(
        "WarningBlinkIntervalSeconds", switchWarningBlinkInterval_);
    if (!std::isfinite(mainTowerSwitchInterval_) || mainTowerSwitchInterval_ <= 0.0f) {
        mainTowerSwitchInterval_ = 30.0f;
    }
    switchWarningLeadSeconds_ = std::isfinite(switchWarningLeadSeconds_)
        ? std::clamp(switchWarningLeadSeconds_, 0.0f, mainTowerSwitchInterval_)
        : 2.0f;
    switchWarningBlinkInterval_ = std::isfinite(switchWarningBlinkInterval_)
        ? std::max(switchWarningBlinkInterval_, 0.01f)
        : 0.5f;
}

MainTower* TowerManager::FindNextSubTower() {
    if (towerCandidates_.empty()) return nullptr;
    for (std::size_t count = 0; count < towerCandidates_.size(); ++count) {
        MainTower* candidate = towerCandidates_[nextCandidateIndex_];
        nextCandidateIndex_ = (nextCandidateIndex_ + 1) % towerCandidates_.size();
        if (std::find(mainTowers_.begin(), mainTowers_.end(), candidate) == mainTowers_.end()) {
            return candidate;
        }
    }
    return nullptr;
}

void TowerManager::UpdateMainTowerSwitch(float _deltaTime) {
    if (mainTowerSwitchSuspended_) return;
    if (mainTowers_.empty() || towerCandidates_.size() <= mainTowers_.size()
        || !std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    mainTowerSwitchTime_ += _deltaTime;
    if (mainTowerSwitchTime_ < mainTowerSwitchInterval_) {
        UpdateSwitchWarning();
        return;
    }
    mainTowerSwitchTime_ = std::fmod(mainTowerSwitchTime_, mainTowerSwitchInterval_);
    if (switchWarningTower_) {
        switchWarningTower_->SetSwitchWarningProgress(-1.0f);
        switchWarningTower_ = nullptr;
    }

    if (nextWarningTower_) {
        nextWarningTower_->SetSwitchWarningProgress(-1.0f);
        nextWarningTower_ = nullptr;
    }

    // 現在は1基だが、一覧で管理しているため将来は複数の防衛対象へ拡張できる。
    for (MainTower*& current : mainTowers_) {
        MainTower* next = FindNextSubTower();
        if (!next) break;
        current->SetDefenseTarget(false);
        next->SetDefenseTarget(true);
        current = next;
        mainTowerSwitched_ = true;
    }
    UpdateSwitchWarning();
}

void TowerManager::UpdateSwitchWarning() {
    const float warningStart = mainTowerSwitchInterval_ - switchWarningLeadSeconds_;
    MainTower* currentTower = !mainTowers_.empty() && mainTowerSwitchTime_ >= warningStart
        ? mainTowers_.front()
        : nullptr;

    if (switchWarningTower_ != currentTower) {
        if (switchWarningTower_) switchWarningTower_->SetSwitchWarningProgress(-1.0f);
        switchWarningTower_ = currentTower;
    }
    MainTower* nextTower = nullptr;
    if (currentTower) {
        for (std::size_t count = 0; count < towerCandidates_.size(); ++count) {
            MainTower* candidate = towerCandidates_[(nextCandidateIndex_ + count) % towerCandidates_.size()];
            if (std::find(mainTowers_.begin(), mainTowers_.end(), candidate) == mainTowers_.end()) {
                nextTower = candidate;
                break;
            }
        }
    }
    if (nextWarningTower_ != nextTower) {
        if (nextWarningTower_) nextWarningTower_->SetSwitchWarningProgress(-1.0f);
        nextWarningTower_ = nextTower;
    }
    if (!switchWarningTower_) return;

    const float warningElapsed = mainTowerSwitchTime_ - warningStart;
    // 0.5秒で透明から半透明へ、その後0.5秒で透明へ戻る。
    const float phase = std::fmod(warningElapsed, switchWarningBlinkInterval_ * 2.0f)
        / switchWarningBlinkInterval_;
    const float opacityProgress = 0.5f - 0.5f * std::cos(phase * 3.14159265358979323846f);
    switchWarningTower_->SetSwitchWarningProgress(opacityProgress);
    if (nextWarningTower_) nextWarningTower_->SetSwitchWarningProgress(opacityProgress);
}

MainTower* TowerManager::ConsumeMainTowerSwitch() {
    if (!mainTowerSwitched_ || mainTowers_.empty()) return nullptr;
    mainTowerSwitched_ = false;
    return mainTowers_.front();
}

void TowerManager::Draw() const {
    for (const auto& tower : towers_) {
        if (tower->IsActive()) {
            tower->Draw();
        }
    }
}

void TowerManager::TakeDamage(float _damage) {
    if (!std::isfinite(_damage) || _damage <= 0.0f) return;
    hp_ = std::max(hp_ - _damage, 0.0f);
    for (MainTower* tower : mainTowers_) tower->PlayDamageFlash();
}

void TowerManager::Heal(float _amount) {
    if (!std::isfinite(_amount) || _amount <= 0.0f) return;
    hp_ = std::min(hp_ + _amount, maxHp_);
}