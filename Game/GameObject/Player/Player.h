#ifndef Player_H_
#define Player_H_
#include "GameObject/GameObject.hpp"

class GameSceneInput;
class MoveComponent;

///
/// 入力デバイスの読み取りは行わず、シーンが用意したGameSceneInputを参照して
/// 「どの入力を自分のどの動きに使うか」だけを解釈する。
/// デバイスの違いはGameSceneInputが吸収済みなので、
/// キーボード／パッドのどちらで操作されているかは知らなくてよい。
class Player : public GameObject {
    /// シーンの入力への参照
    /// 所有権はシーンが持つ。Playerは読み取るだけなのでconst
    const GameSceneInput* input_{ nullptr };

    /// 移動コンポーネント
    /// 所有権はGameObject::components_が持つため、ここは参照用のポインタ
    MoveComponent* move_{ nullptr };

    /// 移動速度の初期値 (units/sec)
    static constexpr float kDefaultMoveSpeed = 5.f;

public:
    Player() = default;
    ~Player() override = default;

    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
    void Debug() override;

    /// @brief 参照する入力を設定する
    /// @param _input シーンが保持する入力。Playerより長生きすること
    /// @note Update()より前に一度だけ呼ぶ
    void SetInput(const GameSceneInput& _input) { input_ = &_input; }

private:
    /// 入力を自分の動きへ反映する
    /// アクションを増やすときはここに解釈を足していく
    void ApplyInput();
}; 

#endif // Player_H_
