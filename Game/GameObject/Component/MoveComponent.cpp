#include "MoveComponent.hpp"

#include <algorithm>

#include "GameObject/GameObject.hpp"
#ifdef _DEBUG
#include "imgui.h"
#endif

void MoveComponent::SetDirection(const Vector3& _direction) {
    const float length = _direction.Length();

    // 長さが1を超えるときだけ正規化する。
    // キーボードの斜め入力(長さ1.41)で速くならず、
    // かつパッドの微入力(長さ0.3など)はそのまま速度の比率として活かせる
    direction_ = (length > 1.f) ? _direction.Normalize() : _direction;
}

void MoveComponent::Update(float _deltaTime) {
    if (!IsEnabled()) return;

    GameObject& owner = Owner();

    const Vector3 velocity = direction_ * speed_;
    owner.SetVelocity(velocity);

    Vector3 position = owner.GetPosition() + velocity * _deltaTime;

    // 移動可能範囲の外へ出ないように丸める
    position.x = std::clamp(position.x, -limit_, limit_);
    position.z = std::clamp(position.z, -limit_, limit_);

    owner.SetPosition(position);
}

void MoveComponent::Debug() {
#ifdef _DEBUG
    if (!ImGui::TreeNode("MoveComponent")) return;

    ImGui::DragFloat("speed", &speed_, 0.1f, 0.f, 100.f);
    ImGui::DragFloat("limit", &limit_, 0.1f, 0.f, 1000.f);
    ImGui::Text("direction : %.2f, %.2f, %.2f", direction_.x, direction_.y, direction_.z);

    ImGui::TreePop();
#endif // _DEBUG
}
