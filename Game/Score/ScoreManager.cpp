#define NOMINMAX

#include "ScoreManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// 終わり際がゆっくりになる補間（演出の減衰に使う）
    float EaseOutCubic(float _t) {
        const float inv = 1.0f - _t;
        return 1.0f - inv * inv * inv;
    }

    /// float の線形補間
    float Lerp(float _start, float _end, float _t) {
        return _start + (_end - _start) * _t;
    }

    /// 色の線形補間
    Vector4 LerpColor(const Vector4& _start, const Vector4& _end, float _t) {
        return {
            Lerp(_start.x, _end.x, _t),
            Lerp(_start.y, _end.y, _t),
            Lerp(_start.z, _end.z, _t),
            Lerp(_start.w, _end.w, _t),
        };
    }
}

void ScoreManager::Initialize() {
    LoadConfig();

    score_ = initialScore_;
    displayScore_ = static_cast<float>(score_);
    punchTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    lastStrength_ = 0.0f;

    // Text は「文字列・位置・サイズ」をまとめて初期化する
    textObject_.Initialize(MakeDisplayString(),
                           textPosition_.x, textPosition_.y, fontSize_);
    ApplyTextSettings();

    // 桁数が固定なのでスコア文字列の幅は一定。
    // その右端をポップアップの右揃え基準にも使うことで、設定を二重に持たなくて済む
    textRightEdge_ = textPosition_.x + EstimateTextWidth(MakeDisplayString(), fontSize_);

    // ポップアップは使い回すので、最初に全部初期化して非表示にしておく
    for (auto& popup : popups_) {
        popup.text.Initialize("", textRightEdge_, popupTopY_, popupFontSize_);
        popup.text.SetColor(popupColor_);
        popup.text.SetVisible(false);
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.baseY = popupTopY_;
        popup.strength = 0.0f;
    }
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

    punchTimer_ = std::max(punchTimer_ - _deltaTime, 0.0f);
    flashTimer_ = std::max(flashTimer_ - _deltaTime, 0.0f);

    UpdatePopups(_deltaTime);
    RefreshText();
}

void ScoreManager::Draw() {
    textObject_.Draw();
    for (auto& popup : popups_) {
        popup.text.Draw();
    }
}

void ScoreManager::AddScore(int32_t _score, int32_t _emphasis) {
    const int32_t before = score_;

    score_ += _score * scoreMultiplier_;

    // スコアがマイナスにならないようにする
    score_ = std::max(score_, 0);

    const int32_t gained = score_ - before;
    if (gained <= 0) {
        return;
    }

    // コンボ倍率が高いほど演出を強くする。1倍なら0、上限倍率なら1になる
    const float range = static_cast<float>(std::max(strengthMax_ - 1, 1));
    lastStrength_ = std::clamp(static_cast<float>(_emphasis - 1) / range, 0.0f, 1.0f);

    // 視線を向けていなくても気付けるよう、輝度の急変と動きを同時に走らせる
    flashTimer_ = flashDuration_;
    punchTimer_ = punchDuration_;
    SpawnGainPopup(gained, lastStrength_);
}

void ScoreManager::Reset() {
    score_ = initialScore_;
    displayScore_ = static_cast<float>(score_);
    punchTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    lastStrength_ = 0.0f;

    for (auto& popup : popups_) {
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.text.SetVisible(false);
    }

    RefreshText();
}

void ScoreManager::SetScore(int32_t _score) {
    score_ = std::max(_score, 0);
}

void ScoreManager::SetVisible(bool _visible) {
    textObject_.SetVisible(_visible);
    for (auto& popup : popups_) {
        popup.text.SetVisible(_visible && popup.active);
    }
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

    if (const auto effect = groups.find("GainEffect"); effect != groups.end()) {
        punchDuration_ = read(effect->second, "PunchDuration", punchDuration_);
        punchScale_ = read(effect->second, "PunchScale", punchScale_);
        punchScaleHigh_ = read(effect->second, "PunchScaleHigh", punchScaleHigh_);
        flashDuration_ = read(effect->second, "FlashDuration", flashDuration_);
        gainColor_ = read(effect->second, "GainColor", gainColor_);
        highGainColor_ = read(effect->second, "HighGainColor", highGainColor_);
        strengthMax_ = read(effect->second, "StrengthMax", strengthMax_);
    }

    if (const auto popup = groups.find("Popup"); popup != groups.end()) {
        popupTopY_ = read(popup->second, "TopY", popupTopY_);
        popupFontSize_ = read(popup->second, "FontSize", popupFontSize_);
        popupFontSizeHigh_ = read(popup->second, "FontSizeHigh", popupFontSizeHigh_);
        popupColor_ = read(popup->second, "Color", popupColor_);
        popupHighColor_ = read(popup->second, "HighColor", popupHighColor_);
        popupRiseDistance_ = read(popup->second, "RiseDistance", popupRiseDistance_);
        popupDuration_ = read(popup->second, "DurationSeconds", popupDuration_);
        popupStackOffset_ = read(popup->second, "StackOffset", popupStackOffset_);
        charWidthRatio_ = read(popup->second, "CharWidthRatio", charWidthRatio_);
    }

    // 不正な値が入っていても破綻しないように補正する
    initialScore_ = std::max(initialScore_, 0);
    fontSize_ = std::max(fontSize_, 1.0f);
    displayDigits_ = std::clamp(displayDigits_, 0, 18);
    countUpSpeed_ = std::max(countUpSpeed_, 0.0f);
    punchDuration_ = std::max(punchDuration_, 0.0f);
    punchScale_ = std::max(punchScale_, 1.0f);
    punchScaleHigh_ = std::max(punchScaleHigh_, punchScale_);
    flashDuration_ = std::max(flashDuration_, 0.0f);
    strengthMax_ = std::max(strengthMax_, 1);
    popupFontSize_ = std::max(popupFontSize_, 1.0f);
    popupFontSizeHigh_ = std::max(popupFontSizeHigh_, popupFontSize_);
    popupDuration_ = std::max(popupDuration_, 0.01f);
    popupRiseDistance_ = std::max(popupRiseDistance_, 0.0f);
    charWidthRatio_ = std::clamp(charWidthRatio_, 0.1f, 2.0f);
}

void ScoreManager::ApplyTextSettings() {
    textObject_.SetPosition(textPosition_.x, textPosition_.y);
    textObject_.SetFontSize(fontSize_);
    textObject_.SetColor(textColor_);
}

void ScoreManager::RefreshText() {
    textObject_.SetText(MakeDisplayString());
    textObject_.SetColor(GetTextColor());

    // 加算した瞬間だけ数字を一回り大きくする。
    // 右上に置いているので、拡大した分だけ左へ寄せて右端が動かないようにする
    const float size = fontSize_ * GetPunchScale();
    textObject_.SetFontSize(size);
    textObject_.SetPosition(
        textRightEdge_ - EstimateTextWidth(MakeDisplayString(), size),
        textPosition_.y);
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

void ScoreManager::UpdatePopups(float _deltaTime) {
    for (auto& popup : popups_) {
        if (!popup.active) {
            continue;
        }

        popup.elapsed += _deltaTime;

        const float t = std::clamp(popup.elapsed / popupDuration_, 0.0f, 1.0f);
        if (t >= 1.0f) {
            popup.active = false;
            popup.text.SetVisible(false);
            continue;
        }

        // スコアの数字へ吸い込まれるように上へ浮かせる。
        // 動き出しを速くすることで、目の端でも動きを捉えやすくする
        const float fontSize = Lerp(popupFontSize_, popupFontSizeHigh_, popup.strength);
        const float width = EstimateTextWidth(popup.text.GetText(), fontSize);
        popup.text.SetPosition(
            textRightEdge_ - width,
            popup.baseY - popupRiseDistance_ * EaseOutCubic(t));

        // 後半に入ってから薄くしていく（前半はしっかり読ませる）
        constexpr float FADE_START = 0.55f;
        const float alpha = t < FADE_START
            ? 1.0f
            : 1.0f - (t - FADE_START) / (1.0f - FADE_START);

        const Vector4 color = LerpColor(popupColor_, popupHighColor_, popup.strength);
        popup.text.SetColor({color.x, color.y, color.z, color.w * alpha});
    }
}

void ScoreManager::SpawnGainPopup(int32_t _gained, float _strength) {
    // 空いているポップアップを探す。全部使用中なら一番古いものを再利用する
    GainPopup* target = nullptr;
    for (auto& popup : popups_) {
        if (!popup.active) {
            target = &popup;
            break;
        }
        if (!target || popup.elapsed > target->elapsed) {
            target = &popup;
        }
    }
    if (!target) {
        return;
    }

    // 連続で倒したときに文字が完全に重ならないよう、表示中の数だけ下へずらす
    int32_t activeCount = 0;
    for (const auto& popup : popups_) {
        if (popup.active && &popup != target) {
            ++activeCount;
        }
    }
    target->baseY = popupTopY_ + static_cast<float>(activeCount) * popupStackOffset_;

    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "+%d", _gained);

    // 倍率が高いほど大きく・暖色にして、大量得点だと一目で分かるようにする
    const float fontSize = Lerp(popupFontSize_, popupFontSizeHigh_, _strength);

    target->text.SetText(buffer);
    target->text.SetFontSize(fontSize);
    target->text.SetColor(LerpColor(popupColor_, popupHighColor_, _strength));
    target->text.SetPosition(
        textRightEdge_ - EstimateTextWidth(buffer, fontSize),
        target->baseY);
    target->text.SetVisible(true);
    target->elapsed = 0.0f;
    target->strength = _strength;
    target->active = true;
}

float ScoreManager::EstimateTextWidth(const std::string& _text, float _fontSize) const {
    // Text クラスは左端基準でしか置けず、実際の字送り幅も取得できない。
    // 桁数が変わっても右端が揃うよう、1文字あたりの平均幅から概算する
    return static_cast<float>(_text.size()) * _fontSize * charWidthRatio_;
}

float ScoreManager::GetPunchScale() const {
    if (punchTimer_ <= 0.0f || punchDuration_ <= 0.0f) {
        return 1.0f;
    }

    // 倍率が高い加算ほど大きく弾ませる
    const float peak = Lerp(punchScale_, punchScaleHigh_, lastStrength_);
    const float t = punchTimer_ / punchDuration_;
    return Lerp(1.0f, peak, EaseOutCubic(t));
}

Vector4 ScoreManager::GetTextColor() const {
    if (flashTimer_ <= 0.0f || flashDuration_ <= 0.0f) {
        return textColor_;
    }

    // 加算直後はアクセント色へ振り、通常色へ戻していく。
    // 倍率が高いときは暖色側へ振れるので、稼ぎの大きさが色でも分かる
    const Vector4 accent = LerpColor(gainColor_, highGainColor_, lastStrength_);
    const float t = flashTimer_ / flashDuration_;
    return LerpColor(textColor_, accent, EaseOutCubic(t));
}
