#include "Tower.hpp"

#include <algorithm>
#include <cmath>

#include "Collision/CollisionAttribute.hpp"

namespace {
    constexpr float SELECTION_ANIMATION_DURATION = 0.6f;
    constexpr float SELECTION_START_SCALE = 1.4f;
    constexpr float SELECTION_END_SCALE = 1.01f;
    constexpr Vector4 SELECTION_COLOR{0.1f, 0.35f, 1.0f, 0.8f};
    constexpr Vector4 NORMAL_COLOR{0.0f, 1.0f, 0.0f, 1.0f};
    constexpr Vector4 CONNECTED_COLOR{0.15f, 0.8f, 1.0f, 1.0f};
}

void Tower::SetHovered(bool _hovered) {
    if (_hovered && !hovered_) selectionAnimationTime_ = 0.0f;
    hovered_ = _hovered;
}

void Tower::SetConnected(bool _connected) {
    connected_ = _connected;
    if (model_) model_->SetColor(connected_ ? CONNECTED_COLOR : NORMAL_COLOR);
}

float Tower::GetSelectionScaleMultiplier() const {
    const float progress = SELECTION_ANIMATION_DURATION > 0.0f
        ? std::clamp(selectionAnimationTime_ / SELECTION_ANIMATION_DURATION, 0.0f, 1.0f)
        : 1.0f;
    const float inverse = 1.0f - progress;
    const float eased = 1.0f - inverse * inverse * inverse;
    return SELECTION_START_SCALE + (SELECTION_END_SCALE - SELECTION_START_SCALE) * eased;
}

Vector3 Tower::GetSelectionCenter() const {
    return position_ + modelOffset_ + colliderOffset_;
}

Vector3 Tower::GetSelectionSize() const {
    return scale_ * 2.0f;
}

void Tower::Initialize() {
    SetModel("Cube");
    SetScale({1.0f, 5.0f, 1.0f});
    model_->SetColor(NORMAL_COLOR);

    selectionModel_ = std::make_unique<Model>();
    selectionModel_->Initialize("Cube");
    selectionModel_->SetEnvironmentTexture("skybox.dds");
    selectionModel_->SetColor(SELECTION_COLOR);

    collider_ = std::make_unique<Collision::Collider>();
    collider_->SetName("Tower")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(this)
        ->AddAttribute(CollisionAttribute::Tower)
        ->AddIgnore(CollisionAttribute::Tower | CollisionAttribute::Enemy)
        ->Enable();
}

void Tower::SetEnemyCollisionEnabled(bool _enabled) {
    if (!collider_) return;
    if (_enabled) collider_->RemoveIgnore(CollisionAttribute::Enemy);
    else collider_->AddIgnore(CollisionAttribute::Enemy);
}

void Tower::Update(float _deltaTime) {
    offset_ = modelOffset_;
    UpdateCollider();
    UpdateModel();
    UpdateSelectionEffect(_deltaTime);
}

void Tower::UpdateSelectionEffect(float _deltaTime) {
    if (!hovered_ || !selectionModel_) return;
    if (std::isfinite(_deltaTime) && _deltaTime > 0.0f) {
        selectionAnimationTime_ += _deltaTime;
        if (selectionAnimationTime_ >= SELECTION_ANIMATION_DURATION) {
            selectionAnimationTime_ = std::fmod(selectionAnimationTime_, SELECTION_ANIMATION_DURATION);
        }
    }
    const float multiplier = GetSelectionScaleMultiplier();
    selectionModel_->SetTranslate(position_ + modelOffset_);
    selectionModel_->SetRotate(rotation_);
    selectionModel_->SetScale(scale_ * multiplier);
    selectionModel_->Update();
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
    if (hovered_ && selectionModel_) selectionModel_->Draw();
}
