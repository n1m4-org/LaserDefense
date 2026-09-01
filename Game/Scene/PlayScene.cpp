#include "PlayScene.hpp"

#include "Camera/Controller/CameraController.hpp"
#include "GameObject/Player/Player.h"
#include "Laser/Laser.hpp"
#include "Pattern/Singleton.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"

PlayScene::PlayScene() = default;
PlayScene::~PlayScene() = default;

void PlayScene::Initialize() {
    constexpr Vector3 towerPosition{3.0f, 0.0f, 4.0f};

    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    if (const auto camera = Singleton<CameraController>::GetInstance()->GetActive()) {
        camera->transform_.translate = {0.0f, 40.0f, -16.5f};
    }

    player_ = std::make_unique<Player>();
    player_->Initialize();
    player_->SetInput(input_);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();
    Tower* tower = towerManager_->AddTower(towerPosition);

    laser_ = std::make_unique<Laser>();
    laser_->Initialize();
    laser_->SetStart(player_.get());
    laser_->AddTarget(tower);

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
    enemyManager_->SetTargetPosition(towerPosition.x, towerPosition.z);

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({10.0f, 10.0f, 1.0f});
}

void PlayScene::Update() {
    input_.Update();

    const float deltaTime = Time::GetDeltaTime();
    player_->Update(deltaTime);
    enemyManager_->Update(deltaTime);
    towerManager_->Update(deltaTime);
    laser_->Update();
    floor_->Update();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();
}
