#include "EnemyTestScene.hpp"

#include "Enemy/EnemyManager.hpp"
#include "Time/Time.hpp"

EnemyTestScene::~EnemyTestScene() = default;

void EnemyTestScene::Initialize() {
    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
}

void EnemyTestScene::Update() {
    enemyManager_->Update(Time::GetDeltaTime());
}

void EnemyTestScene::Draw() {
    enemyManager_->Draw();
}
