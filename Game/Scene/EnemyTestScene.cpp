#include "EnemyTestScene.hpp"

#include "Enemy/EnemyManager.hpp"
#include "Camera/Controller/CameraController.hpp"
#include "Pattern/Singleton.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"

EnemyTestScene::~EnemyTestScene() = default;

void EnemyTestScene::Initialize() {
    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    if (const auto camera = Singleton<CameraController>::GetInstance()->GetActive()) {
        camera->transform_.translate = {0.0f, 40.0f, -16.5f};
    }

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
    enemyManager_->SetTargetPosition(0.0f, 0.0f);

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({10.0f, 10.0f, 1.0f});
}

void EnemyTestScene::Update() {
    enemyManager_->Update(Time::GetDeltaTime());
    floor_->Update();
}

void EnemyTestScene::Draw() {
    enemyManager_->Draw();
    floor_->Draw();
}
