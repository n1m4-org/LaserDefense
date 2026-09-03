#define NOMINMAX

#include "ComboManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// ゲージやフラッシュに使う単色テクスチャ
    const std::string WHITE_TEXTURE = "white_x16.png";

    /// @brief 終わり際がゆっくりになる補間（演出の減衰に使う）
    float EaseOutCubic(float _t) {
        const float inv = 1.0f - _t;
        return 1.0f - inv * inv * inv;
    }

    /// @brief float の線形補間
    float Lerp(float _start, float _end, float _t) {
        return _start + (_end - _start) * _t;
    }

    /// @brief 色の線形補間
    Vector4 LerpColor(const Vector4& _start, const Vector4& _end, float _t) {
        return {
            Lerp(_start.x, _end.x, _t),
            Lerp(_start.y, _end.y, _t),
            Lerp(_start.z, _end.z, _t),
            Lerp(_start.w, _end.w, _t),
        };
    }
}

void ComboManager::Initialize() {
    LoadConfig();

    comboCount_ = 0;
    remainingSeconds_ = 0.0f;
    punchTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    blinkTime_ = 0.0f;
    tierUp_ = false;

    // ゲージは単色テクスチャを引き伸ばして作る。
    // アンカーを左端中央にすることで、幅を変えても左端が動かず中心線もずれない
    for (Sprite* sprite : {&gaugeFrameSprite_, &gaugeFillSprite_, &gaugeFlashSprite_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }

    multiplierText_.Initialize("", multiplierPosition_.x, multiplierPosition_.y, multiplierFontSize_);
    multiplierText_.SetColor(lowTierColor_);

    countText_.Initialize("", countPosition_.x, countPosition_.y, countFontSize_);
    countText_.SetColor(countColor_);

    ApplyGaugeSprites();
    RefreshTexts();
}

void ComboManager::Update(float _deltaTime) {
    // 継続時間を減らし、尽きたらコンボを打ち切る（時間式の途切れ条件）
    if (comboCount_ > 0) {
        remainingSeconds_ = std::max(remainingSeconds_ - _deltaTime, 0.0f);
        if (remainingSeconds_ <= 0.0f) {
            Break();
        }
    }

    punchTimer_ = std::max(punchTimer_ - _deltaTime, 0.0f);
    flashTimer_ = std::max(flashTimer_ - _deltaTime, 0.0f);
    blinkTime_ += _deltaTime;

    if (punchTimer_ <= 0.0f) {
        tierUp_ = false;
    }

    ApplyGaugeSprites();
    RefreshTexts();
}

void ComboManager::Draw() {
    // コンボが繋がっていないときは HUD を出さない（画面を無駄に埋めない）
    if (!visible_ || comboCount_ <= 0) {
        return;
    }

    gaugeFrameSprite_.Draw();
    gaugeFillSprite_.Draw();
    gaugeFlashSprite_.Draw();

    multiplierText_.Draw();
    countText_.Draw();
}

void ComboManager::AddCombo() {
    // 倍率の段が上がったかを判定するため、増やす前の倍率を控えておく
    const int32_t beforeMultiplier = GetMultiplier();

    ++comboCount_;
    remainingSeconds_ = durationSeconds_;

    // 段が上がったときだけ演出を強くして、「倍率が伸びた」ことを区別できるようにする
    tierUp_ = GetMultiplier() > beforeMultiplier;
    punchTimer_ = punchDuration_;
    if (tierUp_) {
        flashTimer_ = flashDuration_;
    }
}

void ComboManager::Break() {
    comboCount_ = 0;
    remainingSeconds_ = 0.0f;
    tierUp_ = false;
}

void ComboManager::Reset() {
    Break();
    punchTimer_ = 0.0f;
    flashTimer_ = 0.0f;

    ApplyGaugeSprites();
    RefreshTexts();
}

int32_t ComboManager::GetMultiplier() const {
    if (killsPerStep_ <= 0) {
        return 1;
    }
    // 例: killsPerStep_=5 なら 0-4 キルで x1、5-9 で x2、10-14 で x3 …
    return std::min(1 + comboCount_ / killsPerStep_, maxMultiplier_);
}

float ComboManager::GetRemainingRatio() const {
    if (durationSeconds_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(remainingSeconds_ / durationSeconds_, 0.0f, 1.0f);
}

void ComboManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Combo", "Combo")) {
        return;
    }

    const auto groups = json->GetGroups("Combo");

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
        durationSeconds_ = read(rule->second, "DurationSeconds", durationSeconds_);
        killsPerStep_ = read(rule->second, "KillsPerStep", killsPerStep_);
        maxMultiplier_ = read(rule->second, "MaxMultiplier", maxMultiplier_);
        const int32_t breakOnTowerReach = read(
            rule->second, "BreakOnTowerReach",
            static_cast<int32_t>(breakOnTowerReach_));
        breakOnTowerReach_ = breakOnTowerReach != 0;
    }

    if (const auto effect = groups.find("Effect"); effect != groups.end()) {
        punchDuration_ = read(effect->second, "PunchDuration", punchDuration_);
        punchScale_ = read(effect->second, "PunchScale", punchScale_);
        tierUpPunchScale_ = read(effect->second, "TierUpPunchScale", tierUpPunchScale_);
        flashDuration_ = read(effect->second, "FlashDuration", flashDuration_);
        flashStrength_ = read(effect->second, "FlashStrength", flashStrength_);
        warningRatio_ = read(effect->second, "WarningRatio", warningRatio_);
        blinkSpeed_ = read(effect->second, "BlinkSpeed", blinkSpeed_);
        blinkStrength_ = read(effect->second, "BlinkStrength", blinkStrength_);
    }

    if (const auto multiplier = groups.find("Multiplier"); multiplier != groups.end()) {
        multiplierPosition_ = read(multiplier->second, "Position", multiplierPosition_);
        multiplierFontSize_ = read(multiplier->second, "FontSize", multiplierFontSize_);
        lowTierColor_ = read(multiplier->second, "LowColor", lowTierColor_);
        highTierColor_ = read(multiplier->second, "HighColor", highTierColor_);
    }

    if (const auto count = groups.find("Count"); count != groups.end()) {
        countLabel_ = read(count->second, "Label", countLabel_);
        countPosition_ = read(count->second, "Position", countPosition_);
        countFontSize_ = read(count->second, "FontSize", countFontSize_);
        countColor_ = read(count->second, "Color", countColor_);
    }

    if (const auto gauge = groups.find("Gauge"); gauge != groups.end()) {
        gaugePosition_ = read(gauge->second, "Position", gaugePosition_);
        gaugeSize_ = read(gauge->second, "Size", gaugeSize_);
        gaugeFrameThickness_ = read(gauge->second, "FrameThickness", gaugeFrameThickness_);
        gaugeFrameColor_ = read(gauge->second, "FrameColor", gaugeFrameColor_);
    }

    // 不正な値が入っていても破綻しないように補正する
    durationSeconds_ = std::max(durationSeconds_, 0.1f);
    killsPerStep_ = std::max(killsPerStep_, 1);
    maxMultiplier_ = std::max(maxMultiplier_, 1);
    punchDuration_ = std::max(punchDuration_, 0.0f);
    punchScale_ = std::max(punchScale_, 1.0f);
    tierUpPunchScale_ = std::max(tierUpPunchScale_, 1.0f);
    flashDuration_ = std::max(flashDuration_, 0.0f);
    flashStrength_ = std::clamp(flashStrength_, 0.0f, 1.0f);
    warningRatio_ = std::clamp(warningRatio_, 0.0f, 1.0f);
    blinkSpeed_ = std::max(blinkSpeed_, 0.0f);
    blinkStrength_ = std::clamp(blinkStrength_, 0.0f, 1.0f);
    multiplierFontSize_ = std::max(multiplierFontSize_, 1.0f);
    countFontSize_ = std::max(countFontSize_, 1.0f);
    gaugeSize_.x = std::max(gaugeSize_.x, 1.0f);
    gaugeSize_.y = std::max(gaugeSize_.y, 1.0f);
    gaugeFrameThickness_ = std::max(gaugeFrameThickness_, 0.0f);
}

void ComboManager::ApplyGaugeSprites() {
    const float punch = GetPunchScale();
    const Vector4 tierColor = GetTierColor();

    // 枠は本体より gaugeFrameThickness_ 分だけ外側へ広げる
    const Vector2 framePosition{gaugePosition_.x - gaugeFrameThickness_, gaugePosition_.y};
    const Vector2 frameSize{
        gaugeSize_.x + gaugeFrameThickness_ * 2.0f,
        (gaugeSize_.y + gaugeFrameThickness_ * 2.0f) * punch};

    gaugeFrameSprite_.SetPosition(framePosition);
    gaugeFrameSprite_.SetSize(frameSize);
    gaugeFrameSprite_.SetColor(gaugeFrameColor_);
    gaugeFrameSprite_.Update();

    // 本体バーは残りの継続時間。減っていく様子がそのまま「切れるまでの猶予」になる
    gaugeFillSprite_.SetPosition(gaugePosition_);
    gaugeFillSprite_.SetSize({gaugeSize_.x * GetRemainingRatio(), gaugeSize_.y * punch});
    gaugeFillSprite_.SetColor(tierColor);
    gaugeFillSprite_.Update();

    // フラッシュはゲージ全体を覆う白。段が上がった瞬間だけ光る
    gaugeFlashSprite_.SetPosition(framePosition);
    gaugeFlashSprite_.SetSize(frameSize);
    gaugeFlashSprite_.SetColor({1.0f, 1.0f, 1.0f, GetFlashAlpha()});
    gaugeFlashSprite_.Update();
}

void ComboManager::RefreshTexts() {
    char buffer[32]{};

    // 倍率は "x3" の形。フォントアトラスが ASCII のみなので '×' ではなく 'x' を使う
    std::snprintf(buffer, sizeof(buffer), "x%d", GetMultiplier());
    multiplierText_.SetText(buffer);
    multiplierText_.SetColor(GetTierColor());

    // 段が上がった瞬間だけ倍率の文字を大きく弾ませる
    multiplierText_.SetFontSize(multiplierFontSize_ * GetPunchScale());

    countText_.SetText(countLabel_ + std::to_string(comboCount_));
    countText_.SetColor(countColor_);
}

Vector4 ComboManager::GetTierColor() const {
    // 倍率が上がるほど寒色から暖色へ寄せる。今どのくらい強いかが色だけで分かる
    const float range = static_cast<float>(std::max(maxMultiplier_ - 1, 1));
    const float t = std::clamp(static_cast<float>(GetMultiplier() - 1) / range, 0.0f, 1.0f);
    Vector4 color = LerpColor(lowTierColor_, highTierColor_, t);

    // 切れかけているときは明滅させる。周辺視野は輝度の変化に反応するので、
    // 視線をコンボ UI へ向けていなくても「もうすぐ切れる」と気付ける
    if (comboCount_ > 0 && GetRemainingRatio() <= warningRatio_ && blinkStrength_ > 0.0f) {
        const float pulse = (std::sin(blinkTime_ * blinkSpeed_) + 1.0f) * 0.5f;
        const float brightness = 1.0f - blinkStrength_ * pulse;
        color.x *= brightness;
        color.y *= brightness;
        color.z *= brightness;
    }

    return color;
}

float ComboManager::GetPunchScale() const {
    if (punchTimer_ <= 0.0f || punchDuration_ <= 0.0f) {
        return 1.0f;
    }

    // 段が上がったときは通常より大きく膨らませて、単なる1キルと区別できるようにする
    const float peak = tierUp_ ? tierUpPunchScale_ : punchScale_;
    const float t = punchTimer_ / punchDuration_;
    return Lerp(1.0f, peak, EaseOutCubic(t));
}

float ComboManager::GetFlashAlpha() const {
    if (flashTimer_ <= 0.0f || flashDuration_ <= 0.0f) {
        return 0.0f;
    }

    // 消えるときはゆるやかにして残像感を出す
    const float t = flashTimer_ / flashDuration_;
    return flashStrength_ * EaseOutCubic(t);
}
