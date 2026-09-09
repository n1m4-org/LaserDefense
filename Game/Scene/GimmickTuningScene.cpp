#define NOMINMAX
#include "GimmickTuningScene.hpp"

#include "Camera/PlayerCamera.hpp"
#include "GameObject/Player/Player.h"
#include "Math/MathUtils.hpp"
#include "Time/Time.hpp"

GimmickTuningScene::GimmickTuningScene()  = default;
GimmickTuningScene::~GimmickTuningScene() = default;

void GimmickTuningScene::Initialize() {
    constexpr Vector3 mainTowerPosition{0.0f, 0.0f, 0.0f};

    player_ = std::make_unique<Player>();
    player_->Initialize();
    player_->SetPosition(mainTowerPosition + Vector3{0.0f, 0.0f, -8.0f});
    player_->SetInput(input_);

    playerCamera_ = std::make_unique<PlayerCamera>();
    playerCamera_->Initialize(*player_);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();
    towerManager_->AddMainTower(mainTowerPosition);

    gimmickManager_ = std::make_unique<GimmickManager>();
    gimmickManager_->Initialize(GimmickContext{
        player_.get(), towerManager_.get(), nullptr, Particle()});

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTexture("white_x16.png");
    floor_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-MathUtils::F_PI * 0.5f, 0.0f, 0.0f});
    floor_->SetScale({40.0f, 40.0f, 1.0f});
    floor_->Update();
}

void GimmickTuningScene::Update() {
    input_.Update();
    const float deltaTime = Time::GetDeltaTime();

    playerCamera_->Update(*player_, deltaTime);
    towerManager_->Update(deltaTime);
    player_->Update(deltaTime);
    gimmickManager_->Update(deltaTime);
}

void GimmickTuningScene::Draw() {
    floor_->Draw();
    player_->Draw();
    towerManager_->Draw();
    gimmickManager_->Draw();
}

void GimmickTuningScene::Debug() {
#ifdef _DEBUG
    gimmickManager_->Debug();
#endif
}
