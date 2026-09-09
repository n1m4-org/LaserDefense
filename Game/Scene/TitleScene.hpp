#ifndef TITLE_SCENE_HPP_
#define TITLE_SCENE_HPP_

#include "IScene.hpp"
#include "Scene/Input/TitleSceneInput.hpp"
#include "Ui/UserInterface.hpp"

/// タイトルシーン
///
/// 見た目とレイアウトは Assets/Data/UI/Title.json が持ち、
/// 実行中に DebugUI の "UI" メニュー（UIエディタ）から編集・保存できる。
/// シーン側の責務は以下の3つで、要素の位置や色には関与しない。
///  - JSON の Events から呼ばれるアクションの登録
///  - JSON の ShowAnim / IdleAnim から参照されるアニメーション関数の登録
///  - Canvas の表示切り替えとシーン遷移
class TitleScene final : public IScene {
    /// 読み込む Canvas 名(= Assets/Data/UI/<名前>.json)
    static constexpr const char* CANVAS_NAME = "Title";

    /// 決定アクションのキー。JSON の Events に書く文字列と一致させること
    static constexpr const char* ACTION_START = "Title.Start";

    /// 中央から伸ばす下線の要素名。JSON の Name と一致させること
    static constexpr const char* UNDERLINE_NAME = "TitleUnderline";

    /// このシーンの入力
    TitleSceneInput input_{};

    /// タイトルUI。エディタからの編集対象でもある
    Ui::Canvas canvas_{};

    /// 下線が伸びるまでの経過秒数
    float underlineElapsed_ = 0.0f;

    /// 伸びきったときの下線の大きさ。JSON(=エディタ)の Size をそのまま使う
    Vector2 underlineFullSize_{};

    /// 下線が伸びきったか。伸びきったあとは要素に触らない
    bool underlineGrown_ = false;

    /// 遷移要求済みか。Canvasとシーン入力が同じフレームに反応する場合の二重実行を防ぐ
    bool transitionRequested_ = false;

public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void Debug() override;

private:
    /// アニメーション関数とアクションを登録してから Canvas を読み込む
    void SetupCanvas();

    /// タイトルを開いたときに下線を中央から左右へ伸ばす
    /// @param _deltaTime 前フレームからの経過秒数
    /// @note 伸ばす長さは JSON の Size が持つので、長さの調整はエディタ側でできる
    void UpdateUnderline(float _deltaTime);

    /// ゲームシーンへの遷移を要求する
    void RequestStart();
};

#endif // TITLE_SCENE_HPP_
