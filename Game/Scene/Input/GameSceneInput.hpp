#ifndef GameSceneInput_HPP_
#define GameSceneInput_HPP_

/// ゲームシーンの入力をまとめて受け取るクラス
///
/// デバイス(キーボード/パッド)の違いを吸収し、
/// 「移動量」「ポーズしたいか」といったゲーム側の意味に変換して公開する。
///
/// 入力の読み取り口をシーンに一本化することで、
/// ・キーコンフィグやパッド対応をこのクラスの中だけで完結できる
/// ・シーンごとに操作の割り当てを変えられる(ポーズ中は移動を受け付けない等)
/// ようになる。
///
/// 参照する側(Player等)はこのクラスへのconst参照を持つ。
/// アクションを増やすときはここに項目を1つ足せばよく、
/// 受け渡しのための関数をシーンや各オブジェクトに追加する必要はない。
class GameSceneInput {
    /// 左右の移動量 [-1.0, 1.0]
    /// 左が負、右が正。パッドのスティックを想定してfloatで保持する
    float moveX_{ 0.f };

    /// 前後の移動量 [-1.0, 1.0]
    /// 手前が負、奥が正
    float moveY_{ 0.f };

    /// このフレームでポーズが押されたか(トリガー)
    bool pause_{ false };

public:
    /// 入力状態を更新する
    /// シーンのUpdateの先頭で1回だけ呼ぶ
    void Update();

    float GetMoveX() const { return moveX_; }
    float GetMoveY() const { return moveY_; }

    /// ポーズが押されたか
    /// 押された瞬間のフレームのみtrue
    bool IsPause() const { return pause_; }

private:
    /// キーボードからの入力を反映する
    void UpdateKeyboard();

    // TODO: パッド対応時に UpdatePad() を追加し、
    //       キーボードとの入力を合成する(絶対値の大きい方を採用するなど)
};

#endif // GameSceneInput_HPP_
