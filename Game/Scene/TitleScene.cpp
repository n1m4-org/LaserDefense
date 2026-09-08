#define NOMINMAX
#include "TitleScene.hpp"

#include "Ui/UiAnimPresets.hpp"

#ifdef _DEBUG
#include "imgui.h"
#endif

TitleScene::TitleScene()  = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
    // 決定時の遷移先。Change()を呼んだタイミングで切り替わる
    next_ = "Play";

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
}

void TitleScene::Update() {
    // 入力の読み取りはシーンで1回だけ
    input_.Update();

    // UI 上のボタンを狙わなくても始められるようにしておく。
    // ボタンのクリックは Canvas 側から ACTION_START として飛んでくる
    if (input_.IsDecide()) {
        RequestStart();
    }
}

void TitleScene::RequestStart() {
    canvas_.SetActive(false);
    Change();
}

void TitleScene::Draw() {
    // UI の描画は Ui::Manager が行うため、シーン側から積むものはない
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
