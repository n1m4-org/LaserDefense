#include "TowerManager.hpp"
#include "MainTower.hpp"

#include "Collision/CollisionAttribute.hpp"
#include "Collision/CollisionManager.h"
#include "Pattern/Singleton.hpp"

Tower* TowerManager::PickTower(const Vector3& _origin, const Vector3& _direction, float _length) const {
    Collision::Ray ray(_origin, _direction, _length);
    // EnemyやLaserではなく、Towerのみを選択する。
    ray.AddIgnore(~CollisionAttribute::Tower);
    const auto collision = Singleton<Collision::Manager>::GetInstance();
    const auto hit = collision->RayCast(&ray);
    if (hit.uuid.empty()) return nullptr;
    const auto* collider = collision->Get(hit.uuid);
    if (!collider) return nullptr;
    for (const auto& tower : towers_) {
        if (tower->IsActive() && collider->GetOwner() == tower.get()) {
            return tower.get();
        }
    }
    return nullptr;
}

void TowerManager::SetHoveredTower(const Tower* _tower) {
    for (const auto& tower : towers_) {
        tower->SetHovered(tower.get() == _tower);
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
