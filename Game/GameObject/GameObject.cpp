#include "GameObject.hpp"

void GameObject::SetPosition(const Vector3& position) {
    position_ = position;
    UpdateTransform();
}

void GameObject::SetRotation(const Vector3& rotation) {
    rotation_ = rotation;
    UpdateTransform();
}

void GameObject::SetScale(const Vector3& scale) {
    scale_ = scale;
    UpdateTransform();
}

void GameObject::UpdateTransform() {
    transform_.translate = position_ + offset_;
    transform_.rotate = rotation_;
    transform_.scale = scale_;
}

void GameObject::ApplyVelocity(float deltaTime) {
    position_ += velocity_ * deltaTime;
}

void GameObject::SetModel(const std::string& _name) {
    model_ = std::make_unique<Model>();
    model_->Initialize(_name);
    model_->SetEnvironmentTexture("skybox.dds");
}

void GameObject::UpdateModel() {
    UpdateTransform();
    // モデル更新
    if (model_){
        model_->SetScale(transform_.scale);
        model_->SetRotate(std::get<Vector3>(transform_.rotate));
        model_->SetTranslate(transform_.translate);
        model_->Update();
    }

    offset_ = {};
}
