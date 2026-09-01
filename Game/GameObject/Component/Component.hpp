#ifndef Component_HPP_
#define Component_HPP_
#include <cassert>

class GameObject;

/// GameObjectに機能を「部品」として追加するためのインターフェース。
/// 生成・所有はGameObject側が行い、コンポーネントは自身の責務だけを持つ。
class Component {
    /// 自分を所有しているGameObject
    /// GameObject::AddComponent()から設定される。所有権は持たない
    GameObject* owner_{ nullptr };

    /// 無効化されている間はUpdateをスキップする
    bool enabled_{ true };

public:
    virtual ~Component() = default;

    /// 所有者を設定する
    /// このコンポーネントを持つGameObject
    /// GameObject::AddComponent()が呼び出す。ユーザーコードから呼ぶ必要はない
    void SetOwner(GameObject* _owner) { owner_ = _owner; }

    /// 初期化処理
    /// 有者が設定された後に呼ばれる
    virtual void Initialize() {}

    /// 更新処理
    /// _deltaTime 前フレームからの経過秒数
    virtual void Update(float _deltaTime) { (void)_deltaTime; }

    /// デバッグUIの描画
    virtual void Debug() {}

    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool _enabled) { enabled_ = _enabled; }

protected:
    /// 所有者への参照を取得
    /// 自分を所有しているGameObject
    GameObject& Owner() const {
        assert(owner_ != nullptr && "Component: owner is not set");
        return *owner_;
    }
};

#endif // Component_HPP_
