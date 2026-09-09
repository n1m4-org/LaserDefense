#include "PauseOverlay.hpp"

#include <cmath>

#include "Input.hpp"
#include "Pattern/Singleton.hpp"
#include "UI/UiAnimPresets.hpp"

namespace {
    /// 読み込む Canvas 名(= Assets/Data/UI/<名前>.json)
    constexpr const char* CANVAS_NAME = "Pause";

    /// 「ゲームに戻る」のアクションキー。JSON の Events に書く文字列と一致させること
    constexpr const char* ACTION_RESUME = "Pause.Resume";

    /// 「タイトルに戻る」のアクションキー
    constexpr const char* ACTION_TO_TITLE = "Pause.ToTitle";
}

void PauseOverlay::Initialize() {
    // アニメーション関数とアクションは Setup(=JSON読み込み)より先に登録する。
    // 読み込み時に ShowAnim / IdleAnim / Events のキーから解決されるため、後からでは効かない
    UiAnimPresets::RegisterAll(canvas_);
    canvas_.RegisterAction(ACTION_RESUME, [this] {
        if (active_ && onResume_) onResume_();
    });
    canvas_.RegisterAction(ACTION_TO_TITLE, [this] {
        if (active_ && onReturnToTitle_) onReturnToTitle_();
    });

    canvas_.Setup(CANVAS_NAME);

    // 読み込み直後は表示状態なので、ポーズするまで閉じておく
    Hide();
}

void PauseOverlay::SetOnResume(std::function<void()> _onResume) {
    onResume_ = std::move(_onResume);
}

void PauseOverlay::SetOnReturnToTitle(std::function<void()> _onReturnToTitle) {
    onReturnToTitle_ = std::move(_onReturnToTitle);
}

int PauseOverlay::FindHoveredIndex() const {
    const auto input = Singleton<Input>::GetInstance();
    if (!input) return -1;

    const Vector2 mouse = input->GetMousePosition();
    const Vector2 canvasPosition = canvas_.GetPosition();

    for (size_t i = 0; i < canvas_.GetElementCount(); ++i) {
        const Ui::Element* element = canvas_.GetElement(i);
        if (!element || !element->IsVisible() || !element->IsSelectable()) continue;

        // 要素は中心が基準。見た目の四角とそのまま同じ範囲で判定する
        const Ui::Canvas::ElementRect rect = canvas_.GetElementRect(i);
        const Vector2 center = canvasPosition + rect.position;
        if (std::fabs(mouse.x - center.x) <= rect.size.x * 0.5f &&
            std::fabs(mouse.y - center.y) <= rect.size.y * 0.5f) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PauseOverlay::Update() {
    if (!active_) return;

    const bool idle = (canvas_.GetPhase() == Ui::Canvas::Phase::Idle);

    const int cursor = FindHoveredIndex();
    if (cursor == blinkIndex_) return;
    blinkIndex_ = cursor;

    // 点滅は合っている要素にだけ付ける。
    // JSON の IdleAnim にしてしまうと、Canvas が Idle へ入るときに全要素へ
    // PlayIdle() を配る仕組みのせいで、開いた直後の1フレームだけ両方光ってしまう
    for (size_t i = 0; i < canvas_.GetElementCount(); ++i) {
        Ui::Element* element = canvas_.GetElement(i);
        if (!element) continue;

        const bool hovered = (static_cast<int>(i) == cursor);
        element->SetIdleFunc(hovered ? UiAnimPresets::BlinkBlueFunc() : Ui::AnimFunc{});

        // 出現アニメーションの最中に再生状態へ触ると Show が完了扱いにならず、
        // phase が Idle まで進まなくなる。付け替えだけ済ませて再生は Idle に任せる
        if (!idle) continue;
        if (hovered) element->PlayIdle();
        else         element->StopAnim();
    }
}

void PauseOverlay::Show() {
    if (active_) {
        return;
    }
    active_ = true;
    // 開き直すたびに付け替えからやり直す。
    // Canvas は Idle に入るとき全要素へ PlayIdle() を配るので、
    // 「どれにも乗っていない(-1)」でも必ず一度は止めに行く必要がある
    blinkIndex_ = NO_BLINK_YET;

    // 一度 Inactive を挟んで Show から始めると、出現アニメーションが頭から再生され、
    // あわせてカーソルの初期化とフォーカスの取得も行われる
    canvas_.SetActive(false);
    canvas_.SetActive(true);
}

void PauseOverlay::Hide() {
    active_ = false;
    canvas_.SetActive(false);
}
