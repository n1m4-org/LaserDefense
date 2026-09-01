#define NOMINMAX
#include "Player.h"

#include <algorithm>
#include <cmath>

#include "GameObject/Component/MoveComponent.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Scene/Input/GameSceneInput.hpp"
#ifdef _DEBUG
#include "imgui.h"
#endif

void Player::Initialize() {
    type_ = Type::PLAYER;

    LoadConfig();
    SetModel(modelName_);

    SetPosition(initialPosition_);
    SetRotation(initialRotation_);
    SetScale(modelScale_);
    model_->SetColor(modelColor_);

    velocity_ = {};
    active_ = true;

    // 機能はコンポーネントとして持たせる
    move_ = AddComponent<MoveComponent>(moveSpeed_);
    move_->SetLimit(moveLimit_);
}

void Player::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Player", "Player")) {
        return;
    }

    const auto groups = json->GetGroups("Player");
    const auto read = []<typename T>(const auto& _group, const std::string & _key, const T & _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) {
            return _fallback;
        }
        if (const auto value = std::get_if<T>(&entry->second)) {
            return *value;
        }
        return _fallback;
    };

    if (const auto appearance = groups.find("Appearance"); appearance != groups.end()) {
        modelName_ = read(appearance->second, "Model", modelName_);
        modelColor_ = read(appearance->second, "Color", modelColor_);
        modelScale_ = read(appearance->second, "Scale", modelScale_);
        modelOffset_ = read(appearance->second, "ModelOffset", modelOffset_);
    }

    if (const auto transform = groups.find("Transform"); transform != groups.end()) {
        initialPosition_ = read(transform->second, "Position", initialPosition_);
        initialRotation_ = read(transform->second, "Rotation", initialRotation_);
    }

    if (const auto movement = groups.find("Movement"); movement != groups.end()) {
        moveSpeed_ = read(movement->second, "Speed", moveSpeed_);
        moveLimit_ = read(movement->second, "Limit", moveLimit_);
    }

    modelScale_.x = std::max(std::abs(modelScale_.x), 0.0001f);
    modelScale_.y = std::max(std::abs(modelScale_.y), 0.0001f);
    modelScale_.z = std::max(std::abs(modelScale_.z), 0.0001f);
    moveSpeed_ = std::max(moveSpeed_, 0.0f);
    moveLimit_ = std::abs(moveLimit_);
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
    offset_ = modelOffset_;
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
