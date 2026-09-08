#ifndef TitleSceneInput_HPP_
#define TitleSceneInput_HPP_

/// タイトルシーンの入力をまとめて受け取るクラス

class TitleSceneInput {
    /// このフレームで決定が押されたか(トリガー)
    bool decide_{ false };

public:
    /// 入力状態を更新する
    /// シーンのUpdateの先頭で1回だけ呼ぶ
    void Update();

    /// 決定が押されたか
    /// 押された瞬間のフレームのみtrue
    bool IsDecide() const { return decide_; }

private:
    /// キーボードからの入力を反映する
    void UpdateKeyboard();

    /// マウスからの入力を反映する
    void UpdateMouse();

    // TODO: パッド対応時に UpdatePad() を追加し、キーボードとの入力を合成する
};

#endif // TitleSceneInput_HPP_
