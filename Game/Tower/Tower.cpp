#include "Tower.hpp"

#include "Collision/CollisionAttribute.hpp"

void Tower::SetHovered(bool _hovered) {
    if (model_) {
        model_->SetColor(_hovered ? Vector4{1.0f, 0.0f, 0.0f, 1.0f}
                                 : Vector4{0.0f, 1.0f, 0.0f, 1.0f});
    }
}

void Tower::Initialize() {
    SetModel("Cube");
    SetScale({1.0f, 5.0f, 1.0f});
    model_->SetColor({0.0f, 1.0f, 0.0f, 1.0f});

    collider_ = std::make_unique<Collision::Collider>();
    collider_->SetName("Tower")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(this)
        ->AddAttribute(CollisionAttribute::Tower)
        ->AddIgnore(CollisionAttribute::Tower)
        ->Enable();
}

void Tower::Update(float _deltaTime) {
    static_cast<void>(_deltaTime);
    offset_ = modelOffset_;
    UpdateCollider();
    UpdateModel();
}

void Tower::UpdateCollider() {
    if (!collider_) {
        return;
    }

    collider_->SetTranslate(position_ + offset_ + colliderOffset_);
    collider_->SetSize(scale_ * 2.0f);
}

void Tower::Draw() {
    if (model_) {
        model_->Draw();
    }
}
