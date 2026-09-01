#include "GameSceneInput.hpp"

#include "Input.hpp"
#include "Pattern/Singleton.hpp"

void GameSceneInput::Update() {
    // 毎フレーム作り直す。デバイスを増やす場合はここで合成する
    moveX_ = 0.0f;
    moveY_ = 0.0f;
    pause_ = false;

    UpdateKeyboard();
}

void GameSceneInput::UpdateKeyboard() {
    const auto input = Singleton<Input>::GetInstance();

    // キーボードはON/OFFしかないため、倒しきった状態(±1.0)として扱う
    if (input->IsPress(DIK_A) || input->IsPress(DIK_LEFT))  moveX_ -= 1.0f;
    if (input->IsPress(DIK_D) || input->IsPress(DIK_RIGHT)) moveX_ += 1.0f;
    if (input->IsPress(DIK_S) || input->IsPress(DIK_DOWN))  moveY_ -= 1.0f;
    if (input->IsPress(DIK_W) || input->IsPress(DIK_UP))    moveY_ += 1.0f;

    // ポーズは押しっぱなしで連続発火しないようにトリガーで取る
    if (input->IsTrigger(DIK_ESCAPE)) pause_ = true;
}
