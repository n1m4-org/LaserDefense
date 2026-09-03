#ifndef TOWER_HPP_
#define TOWER_HPP_

#include "GameObject/GameObject.hpp"
#include "Collision/Collider.h"

class Tower : public GameObject {
    std::unique_ptr<Collision::Collider> collider_;
    Vector3 colliderOffset_{};

protected:
    Vector3 modelOffset_{0.0f, 5.0f, 0.0f};

public:
    virtual void SetHovered(bool _hovered);
    void SetColliderOffset(const Vector3& _offset) { colliderOffset_ = _offset; }
    const Vector3& GetColliderOffset() const { return colliderOffset_; }

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;

private:
    void UpdateCollider();
};

#endif // TOWER_HPP_
