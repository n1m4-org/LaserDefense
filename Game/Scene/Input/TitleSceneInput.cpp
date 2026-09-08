#include "TitleSceneInput.hpp"

#include "Input.hpp"
#include "Pattern/Singleton.hpp"

void TitleSceneInput::Update() {
    // 毎フレーム作り直す。デバイスを増やす場合はここで合成する
    decide_ = false;

    UpdateKeyboard();
    UpdateMouse();
}

void TitleSceneInput::UpdateKeyboard() {
    const auto input = Singleton<Input>::GetInstance();

    // 押しっぱなしで連続しないようにトリガーで取る
    if (input->IsTrigger(DIK_SPACE)) decide_ = true;
}

void TitleSceneInput::UpdateMouse() {
    const auto input = Singleton<Input>::GetInstance();

    // 0 が左ボタン。キーボードと同じくトリガーで取る
    if (input->IsMouseTrigger(0)) decide_ = true;
}
