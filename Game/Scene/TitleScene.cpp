#define NOMINMAX
#include "TitleScene.hpp"

#include <algorithm>
#include <cstddef>

#include "Sound/GameSound.hpp"
#include "Time/Time.hpp"
#include "Ui/UiAnimPresets.hpp"

#ifdef _DEBUG
#include "imgui.h"
#endif

namespace {
    /// タイトルが出てから下線が伸び始めるまでの秒数
    constexpr float UNDERLINE_START_DELAY = 0.0f;

    /// 下線が伸び始めてから伸びきるまでの秒数
    constexpr float UNDERLINE_GROW_DURATION = 1.6f;

    /// 1フレームで進める経過時間の上限(秒)
    constexpr float MAX_STEP_SECONDS = 0.1f;

    /// エンジンのデフォルトFadeと同じ時間
    constexpr float EXIT_FADE_DURATION = 1.0f;

    /// TextはFade用Spriteより後に描画されるため、色を同期して黒へ近づける。
    /// 遷移完了まで黒を維持する必要があるので、アニメーション自体は終了させない。
    bool FadeTextToBlack(float _elapsed, Vector2&, Vector4& _color) {
        const float progress = std::clamp(_elapsed / EXIT_FADE_DURATION, 0.0f, 1.0f);
        const float brightness = 1.0f - progress;
        _color.x *= brightness;
        _color.y *= brightness;
        _color.z *= brightness;
        return false;
    }
} // namespace

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
    // 決定時の遷移先。Change()を呼んだタイミングで切り替わる
    next_ = "Play";

    // 効果音はゲーム全体で1度だけ読み込み、以降のシーンでも読み直さずに使う
    GameSound::Load();
    GameSound::StartLoop(GameSound::Se::TitleBgm);

    // トランジション設定
    entryTransition_ = Transition::Type::Fade;
    exitTransition_ = Transition::Type::Fade;
    transitionRequested_ = false;

    SetupCanvas();
}

void TitleScene::SetupCanvas() {
    // アニメーション関数は Setup(=JSON読み込み)より先に登録する。
    // 読み込み時に ShowAnim / IdleAnim のキーから解決されるため、
    // 後から登録すると JSON で指定しても再生されない
    UiAnimPresets::RegisterAll(canvas_);

    // JSON の Events に書いたキーと対応付ける
    canvas_.RegisterAction(ACTION_START, [this] { RequestStart(); });

    canvas_.Setup(CANVAS_NAME);

    // 読み込み直後は Idle 状態なので、一度 Inactive を挟んで Show から始める。
    // あわせてカーソルの初期化とフォーカスの取得も行われる
    canvas_.SetActive(false);
    canvas_.SetActive(true);

    // Canvas内のTextだけに終了フェード用アニメーションを設定する。
    for (std::size_t i = 0; i < canvas_.GetElementCount(); ++i) {
        Ui::Element* element = canvas_.GetElement(i);
        if (element && element->GetType() == "Text") {
            element->SetHideFunc(FadeTextToBlack);
        }
    }

    // 伸ばす先の長さはエディタが持つ値をそのまま使い、演出はそこへ向かうだけにする
    underlineElapsed_ = 0.0f;
    underlineGrown_ = false;
    underlineFullSize_ = {};
    if (Ui::Element* underline = canvas_.FindElementByName(UNDERLINE_NAME)) {
        underlineFullSize_ = underline->GetSize();
        // 1フレームだけ全長で映らないよう、最初から縮めておく
        underline->SetSize({ 0.0f, underlineFullSize_.y });
    }
}

void TitleScene::Update() {
    // 入力の読み取りはシーンで1回だけ
    input_.Update();

    GameSound::Update();

    UpdateUnderline(Time::GetDeltaTime());

    // UI 上のボタンを狙わなくても始められるようにしておく。
    // ボタンのクリックは Canvas 側から ACTION_START として飛んでくる
    if (input_.IsDecide()) {
        RequestStart();
    }
}

void TitleScene::UpdateUnderline(float _deltaTime) {
    if (underlineGrown_ || underlineFullSize_.x <= 0.0f) return;

    // エディタからの Reload で要素が作り直されるため、ポインタは持たず毎回引き直す
    Ui::Element* underline = canvas_.FindElementByName(UNDERLINE_NAME);
    if (!underline) return;

    // 起動直後の1フレーム目は初期化の分だけ経過時間が大きく(実測 0.47 秒)、
    // そのまま足すと演出が一瞬で終わってしまうので上限を設ける
    underlineElapsed_ += std::clamp(_deltaTime, 0.0f, MAX_STEP_SECONDS);

    // 伸び始めるまでは幅0のまま待つ(SetupCanvas で縮めてある)
    if (underlineElapsed_ < UNDERLINE_START_DELAY) return;

    // 終わり際がゆっくりになる補間。スプライトは中心が基準なので、
    // 幅を変えるだけで中央から左右へ均等に伸びる
    const float t = std::clamp(
        (underlineElapsed_ - UNDERLINE_START_DELAY) / UNDERLINE_GROW_DURATION, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    underline->SetSize({ underlineFullSize_.x * (1.0f - inv * inv * inv), underlineFullSize_.y });

    if (t >= 1.0f) {
        // 伸びきったら以降は触らない。エディタでの長さ変更がそのまま効くようにする
        underline->SetSize(underlineFullSize_);
        underlineGrown_ = true;
    }
}

void TitleScene::RequestStart() {
    if (transitionRequested_) return;
    transitionRequested_ = true;

    // UI のボタンからもキー入力からもここへ合流するので、決定音はこの1箇所でよい
    GameSound::Play(GameSound::Se::Decide);
    for (std::size_t i = 0; i < canvas_.GetElementCount(); ++i) {
        Ui::Element* element = canvas_.GetElement(i);
        if (element && element->GetType() == "Text") {
            element->PlayHide();
        }
    }
    // Canvasは表示したままにして、タイトルの上から終了フェードを重ねる。
    // シーン交換後の破棄はSceneSwitcherへ任せる。
    Change();
}

void TitleScene::Draw() {
    // UI の描画は Ui::Manager が行うため、シーン側から積むものはない
}

void TitleScene::Finalize() {
    GameSound::StopLoop(GameSound::Se::TitleBgm);
}

void TitleScene::Debug() {
#ifdef _DEBUG
    ImGui::Begin("TitleScene");
    ImGui::Text("press SPACE or LEFT CLICK to start");
    ImGui::Text("canvas   : %s", canvas_.IsActive() ? "active" : "inactive");
    ImGui::Text("elements : %zu", canvas_.GetElementCount());
    ImGui::TextDisabled("layout : Assets/Data/UI/Title.json ([UI] menu)");
    ImGui::End();
#endif // _DEBUG
}
