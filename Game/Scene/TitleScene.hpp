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

    /// このシーンの入力
    TitleSceneInput input_{};

    /// タイトルUI。エディタからの編集対象でもある
    Ui::Canvas canvas_{};

public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Debug() override;

private:
    /// アニメーション関数とアクションを登録してから Canvas を読み込む
    void SetupCanvas();

    /// ゲームシーンへの遷移を要求する
    void RequestStart();
};

#endif // TITLE_SCENE_HPP_
