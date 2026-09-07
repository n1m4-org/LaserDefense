#ifndef GAME_SAMPLE_SCENE_HPP
#define GAME_SAMPLE_SCENE_HPP
#include <memory>

#include "IScene.hpp"
#include "Scene/Input/GameSceneInput.hpp"

class Player;

/// シーンの責務は以下の3つ。
///  - 入力をまとめて受け取り、参照を各オブジェクトへ渡す
///  - GameObjectの生成と寿命管理
///  - 更新／描画の呼び出し順の管理
/// 入力を「どう解釈するか」は各オブジェクトの責務なので、シーンは関与しない。
class GameSampleScene : public IScene {
    /// このシーンの入力
    /// player_が参照を持つため、player_より先に宣言すること
    /// (メンバは宣言と逆順に破棄されるので、参照先が先に消えるのを防ぐ)
    GameSceneInput input_{};

    /// プレイヤー
    std::unique_ptr<Player> player_{ nullptr };

    /// ポーズ中か
    bool paused_{ false };

public:
    GameSampleScene();
    ~GameSampleScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Debug() override;

private:
    /// ポーズの切り替え
    void TogglePause();
};

#endif // GAME_SAMPLE_SCENE_HPP
