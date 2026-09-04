#define NOMINMAX

#include "TimeLimitManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// ゲージやフラッシュに使う単色テクスチャ
    const std::string WHITE_TEXTURE = "white_x16.png";

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

void TimeLimitManager::Initialize() {
    LoadConfig();

    remainingSeconds_ = startSeconds_;
    displayRatio_ = GetRemainingRatio();
    fillTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    punchTimer_ = 0.0f;
    blinkTime_ = 0.0f;

    // ゲージは単色テクスチャを引き伸ばして作る。
    // アンカーを左端中央にすることで、幅を変えても左端が動かず中心線もずれない
    for (Sprite* sprite : {&frameSprite_, &ghostSprite_, &fillSprite_, &flashSprite_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }

    valueText_.Initialize(valueLabel_, valuePosition_.x, valuePosition_.y, valueFontSize_);
    valueText_.SetColor(safeColor_);

    // ポップアップは使い回すので、最初に全部初期化して非表示にしておく
    for (auto& popup : popups_) {
        popup.text.Initialize("", popupPosition_.x, popupPosition_.y, popupFontSize_);
        popup.text.SetColor(popupColor_);
        popup.text.SetVisible(false);
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.baseY = popupPosition_.y;
    }

    ApplyGaugeSprites();
    RefreshValueText();
}

void TimeLimitManager::Update(float _deltaTime) {
    if (countingDown_) {
        remainingSeconds_ = std::max(remainingSeconds_ - _deltaTime, 0.0f);
    }

    UpdateGauge(_deltaTime);
    UpdatePopups(_deltaTime);
    ApplyGaugeSprites();
    RefreshValueText();
}

void TimeLimitManager::Draw() {
    // 枠 → ゴースト → 本体 → フラッシュ の順に重ねる
    frameSprite_.Draw();
    ghostSprite_.Draw();
    fillSprite_.Draw();
    flashSprite_.Draw();

    valueText_.Draw();
    for (auto& popup : popups_) {
        popup.text.Draw();
    }
}

void TimeLimitManager::AddTime(float _seconds) {
    if (_seconds <= 0.0f) {
        return;
    }

    // 上限で頭打ちになった分は「増えていない」ので演出にも反映しない
    const float before = remainingSeconds_;
    remainingSeconds_ = std::min(remainingSeconds_ + _seconds, maxSeconds_);

    const float gained = remainingSeconds_ - before;
    if (gained <= 0.0f) {
        return;
    }

    // 周辺視野へ届く演出をまとめて起動する
    flashTimer_ = flashDuration_;   // 輝度の急変
    punchTimer_ = punchDuration_;   // 動き（縦への膨らみ）
    SpawnGainPopup(gained);         // 増えた秒数の明示

    // 本体バーは今の位置から fillDuration_ 秒かけて新しい残量まで伸ばす。
    // 伸びきるまでの差分がゴーストバーとして明るく見えるので「増えた」ことが分かる。
    // 秒数ではなく時間で制御しているため、加算量が小さくても演出の長さは変わらない
    fillStartRatio_ = displayRatio_;
    fillTimer_ = fillDuration_;
}

void TimeLimitManager::Reset() {
    remainingSeconds_ = startSeconds_;
    displayRatio_ = GetRemainingRatio();
    fillTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    punchTimer_ = 0.0f;

    for (auto& popup : popups_) {
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.text.SetVisible(false);
    }

    ApplyGaugeSprites();
    RefreshValueText();
}

float TimeLimitManager::GetRemainingRatio() const {
    if (maxSeconds_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(remainingSeconds_ / maxSeconds_, 0.0f, 1.0f);
}

void TimeLimitManager::SetVisible(bool _visible) {
    valueText_.SetVisible(_visible);
    for (auto& popup : popups_) {
        popup.text.SetVisible(_visible && popup.active);
    }
}

void TimeLimitManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("TimeLimit", "TimeLimit")) {
        return;
    }

    const auto groups = json->GetGroups("TimeLimit");

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

    if (const auto rule = groups.find("Rule"); rule != groups.end()) {
        startSeconds_ = read(rule->second, "StartSeconds", startSeconds_);
        maxSeconds_ = read(rule->second, "MaxSeconds", maxSeconds_);
    }

    if (const auto gauge = groups.find("Gauge"); gauge != groups.end()) {
        gaugePosition_ = read(gauge->second, "Position", gaugePosition_);
        gaugeSize_ = read(gauge->second, "Size", gaugeSize_);
        frameThickness_ = read(gauge->second, "FrameThickness", frameThickness_);
        frameColor_ = read(gauge->second, "FrameColor", frameColor_);
        safeColor_ = read(gauge->second, "SafeColor", safeColor_);
        warningColor_ = read(gauge->second, "WarningColor", warningColor_);
        dangerColor_ = read(gauge->second, "DangerColor", dangerColor_);
        ghostColor_ = read(gauge->second, "GhostColor", ghostColor_);
        warningRatio_ = read(gauge->second, "WarningRatio", warningRatio_);
        dangerRatio_ = read(gauge->second, "DangerRatio", dangerRatio_);
        fillDuration_ = read(gauge->second, "FillDuration", fillDuration_);
    }

    if (const auto effect = groups.find("GainEffect"); effect != groups.end()) {
        flashDuration_ = read(effect->second, "FlashDuration", flashDuration_);
        flashStrength_ = read(effect->second, "FlashStrength", flashStrength_);
        punchDuration_ = read(effect->second, "PunchDuration", punchDuration_);
        punchScale_ = read(effect->second, "PunchScale", punchScale_);
        blinkSpeed_ = read(effect->second, "DangerBlinkSpeed", blinkSpeed_);
        blinkStrength_ = read(effect->second, "DangerBlinkStrength", blinkStrength_);
    }

    if (const auto value = groups.find("Value"); value != groups.end()) {
        valueLabel_ = read(value->second, "Label", valueLabel_);
        valuePosition_ = read(value->second, "Position", valuePosition_);
        valueFontSize_ = read(value->second, "FontSize", valueFontSize_);
        valuePunchScale_ = read(value->second, "PunchScale", valuePunchScale_);
    }

    if (const auto popup = groups.find("Popup"); popup != groups.end()) {
        popupPosition_ = read(popup->second, "Position", popupPosition_);
        popupFontSize_ = read(popup->second, "FontSize", popupFontSize_);
        popupColor_ = read(popup->second, "Color", popupColor_);
        popupRiseDistance_ = read(popup->second, "RiseDistance", popupRiseDistance_);
        popupDuration_ = read(popup->second, "DurationSeconds", popupDuration_);
        popupStackOffset_ = read(popup->second, "StackOffset", popupStackOffset_);
    }

    // 不正な値が入っていても破綻しないように補正する
    maxSeconds_ = std::max(maxSeconds_, 1.0f);
    startSeconds_ = std::clamp(startSeconds_, 0.0f, maxSeconds_);
    gaugeSize_.x = std::max(gaugeSize_.x, 1.0f);
    gaugeSize_.y = std::max(gaugeSize_.y, 1.0f);
    frameThickness_ = std::max(frameThickness_, 0.0f);
    dangerRatio_ = std::clamp(dangerRatio_, 0.0f, 1.0f);
    warningRatio_ = std::clamp(warningRatio_, dangerRatio_, 1.0f);
    fillDuration_ = std::max(fillDuration_, 0.0f);
    flashDuration_ = std::max(flashDuration_, 0.0f);
    flashStrength_ = std::clamp(flashStrength_, 0.0f, 1.0f);
    punchDuration_ = std::max(punchDuration_, 0.0f);
    punchScale_ = std::max(punchScale_, 1.0f);
    blinkSpeed_ = std::max(blinkSpeed_, 0.0f);
    blinkStrength_ = std::clamp(blinkStrength_, 0.0f, 1.0f);
    valueFontSize_ = std::max(valueFontSize_, 1.0f);
    valuePunchScale_ = std::max(valuePunchScale_, 1.0f);
    popupFontSize_ = std::max(popupFontSize_, 1.0f);
    popupDuration_ = std::max(popupDuration_, 0.01f);
    popupRiseDistance_ = std::max(popupRiseDistance_, 0.0f);
}

void TimeLimitManager::UpdateGauge(float _deltaTime) {
    const float target = GetRemainingRatio();

    if (fillTimer_ > 0.0f && fillDuration_ > 0.0f) {
        // 増加したときだけ、開始位置から現在の残量まで一定時間かけて伸ばす。
        // 伸びている間だけゴーストバーとの差が見えるので「増えた」と分かる
        fillTimer_ = std::max(fillTimer_ - _deltaTime, 0.0f);
        const float t = 1.0f - fillTimer_ / fillDuration_;
        displayRatio_ = Lerp(fillStartRatio_, target, EaseOutCubic(t));

        if (displayRatio_ >= target) {
            displayRatio_ = target;
            fillTimer_ = 0.0f;
        }
    } else {
        // 減少は残り時間をそのまま映す（ここを遅らせると残量が読めなくなる）
        fillTimer_ = 0.0f;
        displayRatio_ = target;
    }

    flashTimer_ = std::max(flashTimer_ - _deltaTime, 0.0f);
    punchTimer_ = std::max(punchTimer_ - _deltaTime, 0.0f);
    blinkTime_ += _deltaTime;
}

void TimeLimitManager::UpdatePopups(float _deltaTime) {
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

        // 出現位置から上へ浮かせる。動き出しを速くして目の端でも動きを捉えやすくする
        popup.text.SetPosition(
            popupPosition_.x,
            popup.baseY - popupRiseDistance_ * EaseOutCubic(t));

        // 後半に入ってから薄くしていく（前半はしっかり読ませる）
        constexpr float FADE_START = 0.55f;
        const float alpha = t < FADE_START
            ? 1.0f
            : 1.0f - (t - FADE_START) / (1.0f - FADE_START);
        popup.text.SetColor({popupColor_.x, popupColor_.y, popupColor_.z, popupColor_.w * alpha});
    }
}

void TimeLimitManager::ApplyGaugeSprites() {
    const float punch = GetPunchScale();
    const float ghostRatio = GetRemainingRatio();
    const Vector4 gaugeColor = GetGaugeColor();

    // 枠は本体より frameThickness_ 分だけ外側へ広げる
    const Vector2 framePosition{gaugePosition_.x - frameThickness_, gaugePosition_.y};
    const Vector2 frameSize{
        gaugeSize_.x + frameThickness_ * 2.0f,
        (gaugeSize_.y + frameThickness_ * 2.0f) * punch};

    frameSprite_.SetPosition(framePosition);
    frameSprite_.SetSize(frameSize);
    frameSprite_.SetColor(frameColor_);
    frameSprite_.Update();

    // ゴーストバーは「増えた後の目標値」。本体が追いつくまでの差分が明るく見える
    ghostSprite_.SetPosition(gaugePosition_);
    ghostSprite_.SetSize({gaugeSize_.x * ghostRatio, gaugeSize_.y * punch});
    ghostSprite_.SetColor(ghostColor_);
    ghostSprite_.Update();

    // 本体バーは実際に描いている割合
    fillSprite_.SetPosition(gaugePosition_);
    fillSprite_.SetSize({gaugeSize_.x * displayRatio_, gaugeSize_.y * punch});
    fillSprite_.SetColor(gaugeColor);
    fillSprite_.Update();

    // フラッシュはゲージ全体を覆う白。輝度の急変が周辺視野に一番効く
    flashSprite_.SetPosition(framePosition);
    flashSprite_.SetSize(frameSize);
    flashSprite_.SetColor({1.0f, 1.0f, 1.0f, GetFlashAlpha()});
    flashSprite_.Update();
}

void TimeLimitManager::RefreshValueText() {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.1f", remainingSeconds_);
    valueText_.SetText(valueLabel_ + buffer);

    // 数値もゲージと同じ色にすることで、どちらを見ても同じ状態が読み取れる
    valueText_.SetColor(GetGaugeColor());

    // 時間が増えた瞬間だけ数字を一回り大きくする
    const float punchT = punchDuration_ > 0.0f ? punchTimer_ / punchDuration_ : 0.0f;
    valueText_.SetFontSize(valueFontSize_ * Lerp(1.0f, valuePunchScale_, EaseOutCubic(punchT)));
}

void TimeLimitManager::SpawnGainPopup(float _seconds) {
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
    target->baseY = popupPosition_.y + static_cast<float>(activeCount) * popupStackOffset_;

    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "+%.1fs", _seconds);

    target->text.SetText(buffer);
    target->text.SetFontSize(popupFontSize_);
    target->text.SetColor(popupColor_);
    target->text.SetPosition(popupPosition_.x, target->baseY);
    target->text.SetVisible(true);
    target->elapsed = 0.0f;
    target->active = true;
}

Vector4 TimeLimitManager::GetGaugeColor() const {
    const float ratio = GetRemainingRatio();

    // 残量に応じて 安全 → 警告 → 危険 と色を変える
    Vector4 color = safeColor_;
    if (ratio <= dangerRatio_) {
        color = dangerColor_;
    } else if (ratio <= warningRatio_) {
        // 警告域では危険色へ寄せていき、境目が唐突にならないようにする
        const float range = warningRatio_ - dangerRatio_;
        const float t = range > 0.0f ? (warningRatio_ - ratio) / range : 1.0f;
        color = LerpColor(warningColor_, dangerColor_, std::clamp(t, 0.0f, 1.0f));
    }

    // 危険域では明滅させる。周辺視野は輝度の変化に反応するので、
    // 視線をゲージへ向けていなくても「まずい」ことに気付ける
    if (ratio <= dangerRatio_ && blinkStrength_ > 0.0f) {
        const float pulse = (std::sin(blinkTime_ * blinkSpeed_) + 1.0f) * 0.5f;
        const float brightness = 1.0f - blinkStrength_ * pulse;
        color.x *= brightness;
        color.y *= brightness;
        color.z *= brightness;
    }

    return color;
}

float TimeLimitManager::GetPunchScale() const {
    if (punchTimer_ <= 0.0f || punchDuration_ <= 0.0f) {
        return 1.0f;
    }

    // 残り時間の割合をそのまま強さに使うと「膨らんで戻る」動きになる
    const float t = punchTimer_ / punchDuration_;
    return Lerp(1.0f, punchScale_, EaseOutCubic(t));
}

float TimeLimitManager::GetFlashAlpha() const {
    if (flashTimer_ <= 0.0f || flashDuration_ <= 0.0f) {
        return 0.0f;
    }

    // 消えるときはゆるやかにして残像感を出す
    const float t = flashTimer_ / flashDuration_;
    return flashStrength_ * EaseOutCubic(t);
}
