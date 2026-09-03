#define NOMINMAX

#include "ScoreManager.hpp"

#include <algorithm>
#include <cmath>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

void ScoreManager::Initialize() {
    LoadConfig();

    score_ = initialScore_;
    displayScore_ = static_cast<float>(score_);

    // Text は「文字列・位置・サイズ」をまとめて初期化する
    textObject_.Initialize(MakeDisplayString(),
                           textPosition_.x, textPosition_.y, fontSize_);
    ApplyTextSettings();
}

void ScoreManager::Update(float _deltaTime) {
    const float target = static_cast<float>(score_);

    if (countUpSpeed_ <= 0.0f) {
        // 演出なし: 実スコアをそのまま表示する
        displayScore_ = target;
    } else if (displayScore_ != target) {
        // 表示値を一定速度で実スコアへ近づける（行き過ぎないようクランプする）
        const float step = countUpSpeed_ * _deltaTime;
        const float diff = target - displayScore_;
        displayScore_ += std::clamp(diff, -step, step);
    }

    RefreshText();
}

void ScoreManager::Draw() {
    textObject_.Draw();
}

void ScoreManager::AddScore(int32_t _score) {
    score_ += _score * scoreMultiplier_;

    // スコアがマイナスにならないようにする
    score_ = std::max(score_, 0);
}

void ScoreManager::Reset() {
    score_ = initialScore_;
    displayScore_ = static_cast<float>(score_);
    RefreshText();
}

void ScoreManager::SetScore(int32_t _score) {
    score_ = std::max(_score, 0);
}

void ScoreManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Score", "Score")) {
        return;
    }

    const auto groups = json->GetGroups("Score");

    // キーが存在し、かつ想定した型で入っている場合だけ値を取り出すヘルパー
    const auto read = []<typename T>(const auto& _group, const std::string& _key, const T& _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) {
            return _fallback;
        }
        if (const auto value = std::get_if<T>(&entry->second)) {
            return *value;
        }
        return _fallback;
    };

    if (const auto initial = groups.find("Initial"); initial != groups.end()) {
        initialScore_ = read(initial->second, "Score", initialScore_);
        scoreMultiplier_ = read(initial->second, "Multiplier", scoreMultiplier_);
    }

    if (const auto display = groups.find("Display"); display != groups.end()) {
        label_ = read(display->second, "Label", label_);
        textPosition_ = read(display->second, "Position", textPosition_);
        fontSize_ = read(display->second, "FontSize", fontSize_);
        textColor_ = read(display->second, "Color", textColor_);
        displayDigits_ = read(display->second, "Digits", displayDigits_);
        countUpSpeed_ = read(display->second, "CountUpSpeed", countUpSpeed_);
    }

    // 不正な値が入っていても破綻しないように補正する
    initialScore_ = std::max(initialScore_, 0);
    fontSize_ = std::max(fontSize_, 1.0f);
    displayDigits_ = std::clamp(displayDigits_, 0, 18);
    countUpSpeed_ = std::max(countUpSpeed_, 0.0f);
}

void ScoreManager::ApplyTextSettings() {
    textObject_.SetPosition(textPosition_.x, textPosition_.y);
    textObject_.SetFontSize(fontSize_);
    textObject_.SetColor(textColor_);
}

void ScoreManager::RefreshText() {
    textObject_.SetText(MakeDisplayString());
}

std::string ScoreManager::MakeDisplayString() const {
    // カウントアップ中でも数字が飛ばないよう切り捨てで整数化する
    const int32_t value = static_cast<int32_t>(std::floor(displayScore_));
    std::string digits = std::to_string(value);

    // 指定桁数に満たない分を0で埋める（桁数指定が0以下なら何もしない）
    if (const auto width = static_cast<size_t>(displayDigits_); digits.size() < width) {
        digits.insert(0, width - digits.size(), '0');
    }

    return label_ + digits;
}
