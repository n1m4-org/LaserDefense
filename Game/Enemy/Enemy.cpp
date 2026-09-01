#include "Enemy.hpp"

void Enemy::Initialize() {
    SetModel("Cube");
    SetPosition({0.0f, 0.0f, 0.0f});
    model_->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
}

void Enemy::Update(float _deltaTime) {
    ApplyVelocity(_deltaTime);
    UpdateModel();
}

void Enemy::Draw() {
    if (model_) {
        model_->Draw();
    }
}
