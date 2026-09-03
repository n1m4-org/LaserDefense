#include "MainTower.hpp"

#include "Collision/CollisionAttribute.hpp"

void MainTower::Initialize() {
    Tower::Initialize();
    // 土台は幅6・高さ2。通常タワーと同じ高さ10の柱をその上に置く。
    modelOffset_ = {0.0f, 7.0f, 0.0f};
    baseModel_ = std::make_unique<Model>();
    baseModel_->Initialize("Cube");
    baseModel_->SetEnvironmentTexture("skybox.dds");
    baseModel_->SetScale({3.0f, 1.0f, 3.0f});
    baseCollider_ = std::make_unique<Collision::Collider>();
    baseCollider_->SetName("MainTowerBase")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(static_cast<Tower*>(this))
        ->AddAttribute(CollisionAttribute::Tower)
        ->AddIgnore(CollisionAttribute::Tower)
        ->SetSize(Vector3{6.0f, 2.0f, 6.0f})
        ->Enable();
    SetHovered(false);
}

void MainTower::Update(float _deltaTime) {
    Tower::Update(_deltaTime);
    const Vector3 center = GetPosition() + Vector3{0.0f, 1.0f, 0.0f};
    baseModel_->SetTranslate(center);
    baseModel_->Update();
    baseCollider_->SetTranslate(center + GetColliderOffset());
}

void MainTower::Draw() {
    baseModel_->Draw();
    Tower::Draw();
}

void MainTower::SetHovered(bool _hovered) {
    Tower::SetHovered(_hovered);
    if (baseModel_) {
        baseModel_->SetColor(_hovered ? Vector4{1.0f, 0.0f, 0.0f, 1.0f}
                                     : Vector4{0.2f, 0.6f, 0.2f, 1.0f});
    }
}
