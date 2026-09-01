#include "Enemy.hpp"

void Enemy::SetAppearance(const std::string& _modelName, const Vector3& _scale,
                          const Vector3& _offset, const Vector4& _color) {
    modelName_ = _modelName;
    modelScale_ = _scale;
    modelOffset_ = _offset;
    modelColor_ = _color;
}

void Enemy::SetMovement(const Vector3& _targetPosition, float _moveSpeed) {
    targetPosition_ = {_targetPosition.x, 0.0f, _targetPosition.z};
    moveSpeed_ = _moveSpeed;
}

void Enemy::Initialize() {
    alive_ = true;
    SetModel(modelName_);
    SetPosition({0.0f, 0.0f, 0.0f});
    SetScale(modelScale_);
    model_->SetColor(modelColor_);
}

void Enemy::Update(float _deltaTime) {
    const Vector3 toTarget = targetPosition_ - position_;
    const float distance = toTarget.Length();
    const float moveDistance = moveSpeed_ * _deltaTime;

    if (distance <= moveDistance) {
        SetPosition(targetPosition_);
        SetVelocity({});
        Kill();
    } else {
        SetVelocity(toTarget.Normalize() * moveSpeed_);
        ApplyVelocity(_deltaTime);
    }

    offset_ = modelOffset_;
    UpdateModel();
}

void Enemy::Draw() {
    if (model_) {
        model_->Draw();
    }
}
