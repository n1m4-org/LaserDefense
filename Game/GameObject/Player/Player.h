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

    std::string modelName_{"Cube"};
    Vector4 modelColor_{1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 modelScale_{0.5f, 0.5f, 0.5f};
    Vector3 modelOffset_{0.0f, 0.5f, 0.0f};
    Vector3 initialPosition_{};
    Vector3 initialRotation_{};
    float moveSpeed_ = 5.0f;
    float moveLimit_ = 20.0f;

    // PlaySceneだけで有効化。通常のテストシーンの移動は維持する。
    bool grappleMovement_ = false;
    const GameObject* grappleTarget_ = nullptr;
    float freeAcceleration_ = 35.0f;
    float freeDrag_ = 1.5f;
    float pullStrength_ = 8.0f;
    float radialDamping_ = 4.0f;
    float swingAcceleration_ = 60.0f;
    float connectedDrag_ = 0.3f;
    float orbitRadius_ = 3.0f;
    float maxGrappleSpeed_ = 35.0f;

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
    const Vector3& GetModelOffset() const { return modelOffset_; }
    void EnableGrappleMovement() { grappleMovement_ = true; }
    // シーン所有の接続先。Update前に毎フレーム設定する。
    void SetGrappleTarget(const GameObject* _target) { grappleTarget_ = _target; }

private:
    void LoadConfig();
    void UpdateGrappleMovement(float _deltaTime);

    /// 入力を自分の動きへ反映する
    /// アクションを増やすときはここに解釈を足していく
    void ApplyInput();
}; 

#endif // Player_H_
