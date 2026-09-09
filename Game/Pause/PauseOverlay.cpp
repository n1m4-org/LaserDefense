#include "PauseOverlay.hpp"

#include "UI/UiAnimPresets.hpp"

namespace {
    /// 読み込む Canvas 名(= Assets/Data/UI/<名前>.json)
    constexpr const char* CANVAS_NAME = "Pause";

    /// 「ゲームに戻る」のアクションキー。JSON の Events に書く文字列と一致させること
    constexpr const char* ACTION_RESUME = "Pause.Resume";
}

void PauseOverlay::Initialize() {
    // アニメーション関数とアクションは Setup(=JSON読み込み)より先に登録する。
    // 読み込み時に ShowAnim / IdleAnim / Events のキーから解決されるため、後からでは効かない
    UiAnimPresets::RegisterAll(canvas_);
    canvas_.RegisterAction(ACTION_RESUME, [this] {
        if (active_ && onResume_) onResume_();
    });

    canvas_.Setup(CANVAS_NAME);

    // 読み込み直後は表示状態なので、ポーズするまで閉じておく
    Hide();
}

void PauseOverlay::SetOnResume(std::function<void()> _onResume) {
    onResume_ = std::move(_onResume);
}

void PauseOverlay::Show() {
    if (active_) {
        return;
    }
    active_ = true;

    // 一度 Inactive を挟んで Show から始めると、出現アニメーションが頭から再生され、
    // あわせてカーソルの初期化とフォーカスの取得も行われる
    canvas_.SetActive(false);
    canvas_.SetActive(true);
}

void PauseOverlay::Hide() {
    active_ = false;
    canvas_.SetActive(false);
}
