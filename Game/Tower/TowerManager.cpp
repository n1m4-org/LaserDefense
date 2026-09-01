#include "TowerManager.hpp"

void TowerManager::Initialize() {
    towers_.clear();
}

void TowerManager::AddTower(const Vector3& _position) {
    auto tower = std::make_unique<Tower>();
    tower->Initialize();
    tower->SetPosition(_position);
    towers_.push_back(std::move(tower));
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
