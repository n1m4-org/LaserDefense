#include "EnemyManager.hpp"

#include "Enemy.hpp"

EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize() {
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize();
    enemies_.push_back(std::move(enemy));
}

void EnemyManager::Update(float _deltaTime) {
    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Update(_deltaTime);
        }
    }
}

void EnemyManager::Draw() const {
    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Draw();
        }
    }
}
