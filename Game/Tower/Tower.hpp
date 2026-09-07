#ifndef TOWER_HPP_
#define TOWER_HPP_

#include "GameObject/GameObject.hpp"
#include "Collision/Collider.h"

class Tower : public GameObject {
    std::unique_ptr<Collision::Collider> collider_;
    Vector3 colliderOffset_{};

protected:
    Vector3 modelOffset_{0.0f, 1.0f, 0.0f};
    std::unique_ptr<Model> selectionModel_;
    bool hovered_ = false;
    bool connected_ = false;
    float selectionAnimationTime_ = 0.0f;
    void SetEnemyCollisionEnabled(bool _enabled);
    float GetSelectionScaleMultiplier() const;

public:
    virtual void SetHovered(bool _hovered);
    virtual void SetConnected(bool _connected);
    virtual Vector3 GetSelectionCenter() const;
    virtual Vector3 GetSelectionSize() const;
    void SetColliderOffset(const Vector3& _offset) { colliderOffset_ = _offset; }
    const Vector3& GetColliderOffset() const { return colliderOffset_; }

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;

private:
    void UpdateCollider();
    void UpdateSelectionEffect(float _deltaTime);
};

#endif // TOWER_HPP_
