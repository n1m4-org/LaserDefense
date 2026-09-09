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

    /// 「ゲームに戻る」が押されたときに呼ぶ処理
    std::function<void()> onResume_;

public:
    /// JSON を読み込み、Canvas を閉じた状態で用意する
    void Initialize();

    /// 「ゲームに戻る」が選ばれたときの処理を登録する
    /// @note ボタンから呼ばれるので、Initialize() の後に必ず設定すること
    void SetOnResume(std::function<void()> _onResume);

    /// ポーズシートを開く（表示中に呼んでも何も起きない）
    void Show();

    /// ポーズシートを閉じる
    void Hide();

    /// @brief ポーズ中かどうか
    /// @note ゲーム側の更新を止める判定に使う
    bool IsActive() const { return active_; }
};

#endif // PAUSE_OVERLAY_HPP_
