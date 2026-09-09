#ifndef PAUSE_OVERLAY_HPP_
#define PAUSE_OVERLAY_HPP_

#include <functional>

#include "Ui/UserInterface.hpp"

/// ゲーム中に ESC で開くポーズシート

class PauseOverlay final {
    /// ポーズUI。エディタからの編集対象でもある
    Ui::Canvas canvas_ {};

    /// シートを出しているか
    bool active_ = false;

    /// いま点滅させている要素の番号。カーソルが移ったら付け替える。
    /// -1 は「どれにも乗っていない」を表すので、未設定は NO_BLINK_YET で区別する
    static constexpr int NO_BLINK_YET = -2;
    int blinkIndex_ = NO_BLINK_YET;

    /// 「ゲームに戻る」が押されたときに呼ぶ処理
    std::function<void()> onResume_;

    /// 「タイトルに戻る」が押されたときに呼ぶ処理
    std::function<void()> onReturnToTitle_;

public:
    /// JSON を読み込み、Canvas を閉じた状態で用意する
    void Initialize();

    /// 「ゲームに戻る」が選ばれたときの処理を登録する
    /// @note ボタンから呼ばれるので、Initialize() の後に必ず設定すること
    void SetOnResume(std::function<void()> _onResume);

    /// 「タイトルに戻る」が選ばれたときの処理を登録する
    /// @note ボタンから呼ばれるので、Initialize() の後に必ず設定すること
    void SetOnReturnToTitle(std::function<void()> _onReturnToTitle);

    /// マウスが重なっているボタンを返す。どれにも重なっていなければ -1
    /// @note Canvas のカーソルはボタンから外れても最後の位置に残るため、こちらで判定する
    int FindHoveredIndex() const;

    /// カーソルが合っているボタンだけを点滅させる
    /// @note ポーズ中は毎フレーム呼ぶ。ゲームを止めていても動かしたいので実時間で回す
    void Update();

    /// ポーズシートを開く（表示中に呼んでも何も起きない）
    void Show();

    /// ポーズシートを閉じる
    void Hide();

    /// @brief ポーズ中かどうか
    /// @note ゲーム側の更新を止める判定に使う
    bool IsActive() const { return active_; }
};

#endif // PAUSE_OVERLAY_HPP_
