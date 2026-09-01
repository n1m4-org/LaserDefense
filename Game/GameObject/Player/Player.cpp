#include "Player.h"

#include "GameObject/Component/MoveComponent.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#ifdef _DEBUG
#include "imgui.h"
#endif

void Player::Initialize() {
    type_ = Type::PLAYER;

    SetModel("Cube");

    SetPosition({ 0.f, 0.f, 0.f });
    SetRotation({});
    SetScale(kModelScale);

    velocity_ = {};
    active_   = true;

    // 機能はコンポーネントとして持たせる
    move_ = AddComponent<MoveComponent>(kDefaultMoveSpeed);
}

void Player::ApplyInput() {
    if (!input_) return;

    // 移動: 画面の奥がZ+、右がX+。上下方向(Y)には移動しない
    move_->SetDirection({ input_->GetMoveX(), 0.f, input_->GetMoveY() });

    // アクションを増やす場合はここへ追加する。
    // 例: if (input_->IsConnect()) connect_->ToggleLine();
}

void Player::Update(float _deltaTime) {
    if (!active_) return;

    ApplyInput();

    UpdateComponents(_deltaTime);
    offset_ = kModelOffset;
    UpdateModel();
}

void Player::Draw() {
    if (!active_) return;

    if (model_) model_->Draw();
}

void Player::Debug() {
#ifdef _DEBUG
    ImGui::Begin("Player");

    ImGui::DragFloat3("position", &position_.x, 0.01f);
    ImGui::DragFloat3("rotation", &rotation_.x, 0.01f);
    ImGui::DragFloat3("scale", &scale_.x, 0.01f);
    ImGui::Checkbox("active", &active_);

    ImGui::Separator();
    DebugComponents();

    ImGui::End();
#endif // _DEBUG
}
