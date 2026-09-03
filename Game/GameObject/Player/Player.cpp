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

    if (const auto grapple = groups.find("Grapple"); grapple != groups.end()) {
        const auto parameter = [&](const char* _key, float _fallback) {
            const float value = read(grapple->second, _key, _fallback);
            return std::isfinite(value) ? std::max(value, 0.0f) : _fallback;
        };
        freeAcceleration_ = parameter("FreeAcceleration", freeAcceleration_);
        freeDrag_ = parameter("FreeDrag", freeDrag_);
        pullStrength_ = parameter("PullStrength", pullStrength_);
        radialDamping_ = parameter("RadialDamping", radialDamping_);
        swingAcceleration_ = parameter("SwingAcceleration", swingAcceleration_);
        connectedDrag_ = parameter("ConnectedDrag", connectedDrag_);
        orbitRadius_ = std::max(parameter("OrbitRadius", orbitRadius_), 0.01f);
        maxGrappleSpeed_ = parameter("MaxSpeed", maxGrappleSpeed_);
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

    move_->SetEnabled(!grappleMovement_);
    if (grappleMovement_) UpdateGrappleMovement(_deltaTime);
    else ApplyInput();

    UpdateComponents(_deltaTime);
    offset_ = modelOffset_;
    UpdateModel();
}

void Player::UpdateGrappleMovement(float _deltaTime) {
    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;

    Vector3 direction{};
    if (input_) direction = {input_->GetMoveX(), 0.0f, input_->GetMoveY()};
    if (direction.Length() > 1.0f) direction = direction.Normalize();
    const bool connected = grappleTarget_ && grappleTarget_->IsActive();
    const auto dotXZ = [](const Vector3& _a, const Vector3& _b) {
        return _a.x * _b.x + _a.z * _b.z;
    };

    // 長いフレームで飛び越さないよう時間を制限し、120Hz以下の刻みに分割する。
    const float elapsed = std::min(_deltaTime, 0.1f);
    const int steps = static_cast<int>(std::ceil(elapsed * 120.0f));
    const float dt = elapsed / static_cast<float>(steps);
    velocity_.y = 0.0f;
    position_.y = 0.0f;
    for (int i = 0; i < steps; ++i) {
        const float previousSpeed = velocity_.Length();
        Vector3 acceleration = direction * freeAcceleration_;
        if (connected) {
            Vector3 toTarget = grappleTarget_->GetPosition() - position_;
            toTarget.y = 0.0f;
            const float distance = toTarget.Length();
            // 中心一致でもゼロ除算せず、一定方向へ離れる。
            const Vector3 inward = distance > 0.0001f
                ? toTarget * (1.0f / distance) : Vector3{1.0f, 0.0f, 0.0f};
            const Vector3 tangentInput = direction - inward * dotXZ(direction, inward);
            // ばね状の引力。半径内では押し戻し、横向きの慣性は保持する。
            const float pull = (distance - orbitRadius_) * pullStrength_
                - dotXZ(velocity_, inward) * radialDamping_;
            acceleration = inward * pull + tangentInput * swingAcceleration_;
        }
        velocity_ += acceleration * dt;
        const float damping = std::exp(-(connected ? connectedDrag_ : freeDrag_) * dt);
        velocity_ = velocity_ * damping;

        // 解放時は高速の慣性を保つ。通常入力だけではSpeedを超えて加速しない。
        const float speedLimit = connected ? maxGrappleSpeed_
            : std::max(moveSpeed_, previousSpeed * damping);
        const float speed = velocity_.Length();
        if (speed > speedLimit && speed > 0.0001f) velocity_ = velocity_ * (speedLimit / speed);
        position_ += velocity_ * dt;
        if (position_.x < -moveLimit_ || position_.x > moveLimit_) {
            position_.x = std::clamp(position_.x, -moveLimit_, moveLimit_);
            velocity_.x = 0.0f;
        }
        if (position_.z < -moveLimit_ || position_.z > moveLimit_) {
            position_.z = std::clamp(position_.z, -moveLimit_, moveLimit_);
            velocity_.z = 0.0f;
        }
    }
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
