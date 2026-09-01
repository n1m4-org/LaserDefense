#include "GameSampleScene.hpp"

#include "GameObject/Player/Player.h"
#include "src/Time/Time.hpp"
#ifdef _DEBUG
#include "imgui.h"
#endif

// Playerが不完全型のままでは std::unique_ptr のデストラクタを生成できないため、
// Player.h をインクルードしたこの翻訳単位で定義する
GameSampleScene::GameSampleScene()  = default;
GameSampleScene::~GameSampleScene() = default;

void GameSampleScene::Initialize() {
    player_ = std::make_unique<Player>();
    player_->Initialize();

    // 入力への参照を渡す。以降シーンから値を配る必要はない
    player_->SetInput(input_);

    paused_ = false;
}

void GameSampleScene::Update() {
    // 入力の読み取りはシーンで1回だけ。
    // 参照はInitializeで渡してあるので、更新すれば各オブジェクトから見える
    input_.Update();

    if (input_.IsPause()) {
        TogglePause();
    }

    // ポーズ中はゲームオブジェクトの更新を止める
    if (paused_) return;

    const float deltaTime = Time::GetDeltaTime();

    player_->Update(deltaTime);
}

void GameSampleScene::Draw() {
    player_->Draw();
}

void GameSampleScene::Debug() {
#ifdef _DEBUG
    ImGui::Begin("GameSampleScene");
    ImGui::Text("paused : %s", paused_ ? "true" : "false");
    ImGui::Text("moveX  : %.2f", input_.GetMoveX());
    ImGui::Text("moveY  : %.2f", input_.GetMoveY());
    ImGui::End();

    player_->Debug();
#endif // _DEBUG
}

void GameSampleScene::TogglePause() {
    paused_ = !paused_;

    // TODO: ポーズシーン(またはポーズ用UI)への切り替えに置き換える。
    //       現状は更新を止めるだけの暫定実装
}
