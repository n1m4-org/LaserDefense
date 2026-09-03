#include "PlayScene.hpp"

#include "Camera/Controller/CameraController.hpp"
#include "GameObject/Player/Player.h"
#include "Laser/Laser.hpp"
#include "Light/LightManager.hpp"
#include "Pattern/Singleton.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"

PlayScene::PlayScene() = default;
PlayScene::~PlayScene() = default;

void PlayScene::Initialize() {
    constexpr Vector3 towerPosition{3.0f, 0.0f, 4.0f};
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    if (const auto camera = Singleton<CameraController>::GetInstance()->GetActive()) {
        camera->transform_.translate = {0.0f, 40.0f, -16.5f};
    }

    player_ = std::make_unique<Player>();
    player_->Initialize();
    player_->SetInput(input_);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();
    Tower* tower = towerManager_->AddTower(towerPosition);

    laser_ = std::make_unique<Laser>();
    laser_->Initialize(Particle());
    laser_->SetStart(player_.get(), player_->GetModelOffset().y);
    laser_->SetTarget(tower);

    // スコアと制限時間は EnemyManager より先に用意し、撃破報酬の加算先として渡しておく
    scoreManager_ = std::make_unique<ScoreManager>();
    scoreManager_->Initialize();

    timeLimitManager_ = std::make_unique<TimeLimitManager>();
    timeLimitManager_->Initialize();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
    enemyManager_->SetTargetPosition(towerPosition.x, towerPosition.z);
    enemyManager_->SetScoreManager(scoreManager_.get());
    enemyManager_->SetTimeLimitManager(timeLimitManager_.get());

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTexture("white_x16.png");
    floor_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({10.0f, 10.0f, 1.0f});
}

void PlayScene::Update() {
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    input_.Update();

    const float deltaTime = Time::GetDeltaTime();
    player_->Update(deltaTime);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);
    enemyManager_->Update(deltaTime);
    towerManager_->Update(deltaTime);
    laser_->Update();
    scoreManager_->Update(deltaTime);
    timeLimitManager_->Update(deltaTime);
    floor_->Update();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();

    // UI は 3D の描画がすべて終わったあとに重ねる
    scoreManager_->Draw();
    timeLimitManager_->Draw();
}
