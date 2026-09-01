#include "Laser.hpp"

#include <algorithm>

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

        for (const Endpoint& target : targets_) {
            if (target.object && target.object->IsActive()) {
                const Vector3 targetPosition =
                    target.object->GetPosition() + Vector3{0.0f, target.lineHeight, 0.0f};
                line_->AddLine(startPosition, targetPosition);
            }
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

void Laser::AddTarget(const GameObject* _object, float _lineHeight) {
    if (!_object) {
        return;
    }

    const auto found = std::find_if(targets_.begin(), targets_.end(), [_object](const Endpoint& _target) {
        return _target.object == _object;
    });

    if (found == targets_.end()) {
        targets_.push_back({_object, _lineHeight});
    } else {
        found->lineHeight = _lineHeight;
    }
}

void Laser::RemoveTarget(const GameObject* _object) {
    std::erase_if(targets_, [_object](const Endpoint& _target) {
        return _target.object == _object;
    });
}

void Laser::ClearTargets() {
    targets_.clear();
}
