#include "Laser.hpp"

#include "GameObject/GameObject.hpp"
#include "Line.hpp"

Laser::Laser() = default;
Laser::~Laser() = default;

void Laser::Initialize() {
    line_ = std::make_unique<Line>();
    line_->Initialize();
    line_->SetName("Laser");
    line_->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
}

void Laser::Update() {
    if (!line_) {
        return;
    }

    line_->Clear();

    if (start_.object && start_.object->IsActive()) {
        const Vector3 startPosition = start_.object->GetPosition() + Vector3{0.0f, start_.lineHeight, 0.0f};

        if (target_.object && target_.object->IsActive()) {
            const Vector3 targetPosition =
                target_.object->GetPosition() + Vector3{0.0f, target_.lineHeight, 0.0f};
            line_->AddLine(startPosition, targetPosition);
        }
    }

    line_->Update();
}

void Laser::Draw() const {
    if (line_) {
        line_->Draw();
    }
}

void Laser::SetStart(const GameObject* _object, float _lineHeight) {
    start_ = {_object, _lineHeight};
}

void Laser::SetTarget(const GameObject* _object, float _lineHeight) {
    target_ = {_object, _lineHeight};
}

void Laser::ClearTarget() {
    target_ = {};
}
