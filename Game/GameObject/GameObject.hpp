#ifndef GameObject_HPP_
#define GameObject_HPP_
#include <memory>

#include "Model.hpp"
#include "Math/Vector3.hpp"
#include "Math/Transform.hpp"

class GameObject {
    enum class Type {
        PLAYER,
        ENEMY,

        NONE
    };

    Transform transform_{};

protected:
    std::unique_ptr<Model> model_{ nullptr };
    Vector3 position_{0.f, 1.f, 0.f};
    Vector3 rotation_{};
    Vector3 scale_{1.f, 1.f, 1.f};
    Vector3 offset_ {};
    Vector3 velocity_{};
    bool active_{true};

    Type type_{ Type::NONE };

public:
    virtual ~GameObject() = default;
    virtual void Initialize() = 0;
    // 基本更新・描画
    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;
    virtual void Debug() {}

    // Transform関連
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetRotation() const { return rotation_; }
    const Vector3& GetScale() const { return scale_; }
    const Transform& GetTransform() const { return transform_; }

    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);

    // Velocity関連
    const Vector3& GetVelocity() const { return velocity_; }
    void SetVelocity(const Vector3& velocity) { velocity_ = velocity; }
    void AddVelocity(const Vector3& velocity) { velocity_ += velocity; }

    bool IsActive() const { return active_; }
    void SetActive(bool active) { active_ = active; }

    Type GetType() const { return type_; }

protected:
    void UpdateModel();

    /// <summary>
    /// velocityを位置に適用（継承先のUpdate内で呼び出す）
    /// </summary>
    void ApplyVelocity(float deltaTime);

    void SetModel(const std::string& _name);

private:
    void UpdateTransform();
}; // class GameObject

#endif // GameObject_HPP_
