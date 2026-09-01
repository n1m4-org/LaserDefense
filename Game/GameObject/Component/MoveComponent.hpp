#ifndef MoveComponent_HPP_
#define MoveComponent_HPP_
#include "Component.hpp"
#include "Math/Vector3.hpp"

/// 「どちらへ動くか」を外部から受け取り、速度と位置の更新だけを行う。
/// 入力の読み取りは行わないため、プレイヤー以外(敵など)でも再利用できる。
class MoveComponent : public Component {
    /// 移動方向
    /// 長さは0〜1。長さがそのまま速度の比率になる(0なら停止、1で最高速)
    Vector3 direction_{};

    /// 移動速度 (units/sec)
    float speed_{ 5.f };

    /// 原点からの移動可能範囲(XZ平面)
    float limit_{ 20.f };

public:
    MoveComponent() = default;

    /// 速度を指定して生成
    /// _speed 移動速度 (units/sec)
    explicit MoveComponent(float _speed) : speed_(_speed) {}

    void Update(float _deltaTime) override;
    void Debug() override;

    /// 移動方向を設定
    /// _direction 移動方向。長さが1を超える場合のみ内部で正規化する
    void SetDirection(const Vector3& _direction);

    const Vector3& GetDirection() const { return direction_; }

    float GetSpeed() const { return speed_; }
    void SetSpeed(float _speed) { speed_ = _speed; }

    float GetLimit() const { return limit_; }
    void SetLimit(float _limit) { limit_ = _limit; }
};

#endif // MoveComponent_HPP_
