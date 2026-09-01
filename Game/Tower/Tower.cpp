#include "Tower.hpp"

void Tower::Initialize() {
    SetModel("Cube");
    SetScale({1.0f, 5.0f, 1.0f});
    model_->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
}

void Tower::Update(float _deltaTime) {
    static_cast<void>(_deltaTime);
    offset_ = {0.0f, 5.0f, 0.0f};
    UpdateModel();
}

void Tower::Draw() {
    if (model_) {
        model_->Draw();
    }
}
