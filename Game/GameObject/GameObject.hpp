#ifndef GameObject_HPP_
#define GameObject_HPP_
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include "Model.hpp"
#include "Math/Vector3.hpp"
#include "Math/Transform.hpp"
#include "Component/Component.hpp"

class GameObject {
public:
    enum class Type {
        PLAYER,
        ENEMY,

        NONE
    };

private:
    Transform transform_{};

protected:
    std::unique_ptr<Model> model_{ nullptr };
    Vector3 position_{0.f, 1.f, 0.f};
    Vector3 rotation_{};
    Vector3 scale_{1.f, 1.f, 1.f};
    Vector3 offset_ {};
    Vector3 velocity_{};
    bool active_{true};

    /// このGameObjectが持つコンポーネント群
    /// 追加された順に保持し、所有権もここが持つ
    std::vector<std::unique_ptr<Component>> components_{};

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

    /// @brief コンポーネントを生成して追加する
    /// @tparam T 追加するコンポーネントの型
    /// @param _args Tのコンストラクタへ渡す引数
    /// @return 生成したコンポーネントへのポインタ(所有権はGameObjectが持つ)
    /// @note 追加と同時にSetOwner()とInitialize()が呼ばれる
    template<class T, class... Args>
        requires std::derived_from<T, Component>
    T* AddComponent(Args&&... _args) {
        auto component = std::make_unique<T>(std::forward<Args>(_args)...);
        T* raw = component.get();

        raw->SetOwner(this);
        raw->Initialize();

        components_.emplace_back(std::move(component));
        return raw;
    }

    /// @brief 型を指定してコンポーネントを検索する
    /// @tparam T 探すコンポーネントの型
    /// @return 最初に見つかったコンポーネント。存在しない場合はnullptr
    template<class T>
        requires std::derived_from<T, Component>
    T* GetComponent() const {
        for (const auto& component : components_) {
            if (auto* found = dynamic_cast<T*>(component.get())) return found;
        }
        return nullptr;
    }

protected:
    void UpdateModel();

    /// @brief 保持している全コンポーネントのUpdateを追加順に呼ぶ
    /// @param _deltaTime 前フレームからの経過秒数
    /// @note 処理順が重要な場合は継承先で個別に呼び出すこと
    void UpdateComponents(float _deltaTime);

    /// @brief 保持している全コンポーネントのDebugを呼ぶ
    void DebugComponents();

    /// <summary>
    /// velocityを位置に適用（継承先のUpdate内で呼び出す）
    /// </summary>
    void ApplyVelocity(float deltaTime);

    void SetModel(const std::string& _name);

private:
    void UpdateTransform();
}; // class GameObject

#endif // GameObject_HPP_
