#define NOMINMAX
#include "TowerManager.hpp"
#include "MainTower.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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
}

Tower* TowerManager::AddTower(const Vector3& _position) {
    auto tower = std::make_unique<Tower>();
    tower->Initialize();
    tower->SetPosition(_position);
    Tower* addedTower = tower.get();
    towers_.push_back(std::move(tower));
    return addedTower;
}

MainTower* TowerManager::AddMainTower(const Vector3& _position) {
    auto tower = std::make_unique<MainTower>();
    tower->Initialize();
    tower->SetPosition(_position);
    tower->Update(0.0f);
    MainTower* addedTower = tower.get();
    towers_.push_back(std::move(tower));
    return addedTower;
}

void TowerManager::Update(float _deltaTime) {
    for (const auto& tower : towers_) {
        if (tower->IsActive()) {
            tower->Update(_deltaTime);
        }
    }
}

void TowerManager::Draw() const {
    for (const auto& tower : towers_) {
        if (tower->IsActive()) {
            tower->Draw();
        }
    }
}
