#ifndef ENEMY_HPP_
#define ENEMY_HPP_

#include "GameObject/GameObject.hpp"

class Enemy final : public GameObject {
    std::string modelName_{"Cube"};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.25f, 0.0f};
    Vector4 modelColor_{1.0f, 0.0f, 0.0f, 1.0f};
    Vector3 targetPosition_{};
    float moveSpeed_ = 1.0f;
    bool alive_ = true;

public:
    void SetAppearance(const std::string& _modelName, const Vector3& _scale,
                       const Vector3& _offset, const Vector4& _color);
    void SetMovement(const Vector3& _targetPosition, float _moveSpeed);
    bool IsAlive() const { return alive_; }
    void Kill() { alive_ = false; }

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
};

#endif // ENEMY_HPP_
