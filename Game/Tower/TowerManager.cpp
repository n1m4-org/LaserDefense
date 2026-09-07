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
    LoadConfig();
}

Tower* TowerManager::AddTower(const Vector3& _position) {
    // すべての設置枠を将来メイン化できる候補として生成する。
    auto tower = std::make_unique<MainTower>();
    tower->Initialize();
    tower->SetDefenseTarget(false);
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
    const auto group = groups.find("Switch");
    if (group == groups.end()) return;
    const auto entry = group->second.find("IntervalSeconds");
    if (entry == group->second.end()) return;
    if (const auto floatValue = std::get_if<float>(&entry->second)) {
        mainTowerSwitchInterval_ = *floatValue;
    } else if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
        mainTowerSwitchInterval_ = static_cast<float>(*integerValue);
    }
    if (!std::isfinite(mainTowerSwitchInterval_) || mainTowerSwitchInterval_ <= 0.0f) {
        mainTowerSwitchInterval_ = 30.0f;
    }
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
    if (mainTowers_.empty() || towerCandidates_.size() <= mainTowers_.size()
        || !std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    mainTowerSwitchTime_ += _deltaTime;
    if (mainTowerSwitchTime_ < mainTowerSwitchInterval_) return;
    mainTowerSwitchTime_ = std::fmod(mainTowerSwitchTime_, mainTowerSwitchInterval_);

    // 現在は1基だが、一覧で管理しているため将来は複数の防衛対象へ拡張できる。
    for (MainTower*& current : mainTowers_) {
        MainTower* next = FindNextSubTower();
        if (!next) break;
        current->SetDefenseTarget(false);
        next->SetDefenseTarget(true);
        current = next;
        mainTowerSwitched_ = true;
    }
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
