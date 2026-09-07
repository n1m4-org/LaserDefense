#define NOMINMAX

#include "TowerHpGauge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "MainTower.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"

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

void TowerHpGauge::Initialize() {
    LoadConfig();

    // ゲージは単色テクスチャを引き伸ばして作る。
    // アンカーを左端中央にすることで、幅を変えても左端が動かず中心線もずれない
    for (Sprite* sprite : {&frameSprite_, &trailSprite_, &fillSprite_, &flashSprite_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }

    // 画面フラッシュだけは画面全体を覆うので、左上を基準にする
    screenFlashSprite_.Initialize(WHITE_TEXTURE);
    screenFlashSprite_.SetAnchorPoint({0.0f, 0.0f});
    screenFlashSprite_.SetPosition({0.0f, 0.0f});
    const auto screen = Singleton<Screen>::GetInstance();
    screenFlashSprite_.SetSize({screen->Width(), screen->Height()});

    labelText_.Initialize(label_, labelPosition_.x, labelPosition_.y, labelFontSize_);
    labelText_.SetColor(labelColor_);

    valueText_.Initialize("", valueRightX_, valuePositionY_, valueFontSize_);
    valueText_.SetColor(safeColor_);

    // ポップアップは使い回すので、最初に全部初期化して非表示にしておく
    for (auto& popup : popups_) {
        popup.text.Initialize("", popupCenterX_, popupPositionY_, popupFontSize_);
        popup.text.SetColor(popupColor_);
        popup.text.SetVisible(false);
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.baseX = popupCenterX_;
        popup.baseY = popupPositionY_;
    }

    Reset();
}

void TowerHpGauge::SetTarget(const MainTower* _tower) {
    target_ = _tower;
    // 対象が変わったら差分検知の基準も取り直す（初回にダメージと誤検知しないため）
    lastHp_ = target_ ? target_->GetHp() : -1.0f;
    Reset();
}

void TowerHpGauge::Update(float _deltaTime) {
    if (!target_) {
        return;
    }

    // HP の実データはタワーが持っている。ここでは前フレームとの差だけを見る
    const float hp = target_->GetHp();
    if (lastHp_ >= 0.0f && hp < lastHp_) {
        OnDamaged(lastHp_ - hp);
    }
    lastHp_ = hp;

    UpdateGauge(_deltaTime);
    UpdatePopups(_deltaTime);
    ApplyGaugeSprites();
    RefreshValueText();
}

void TowerHpGauge::Draw() {
    if (!target_ || !visible_) {
        return;
    }

    // 画面フラッシュはいちばん奥。ゲージや数値の上に被せると読めなくなる
    screenFlashSprite_.Draw();

    // 枠 → トレイル → 本体 → フラッシュ の順に重ねる
    frameSprite_.Draw();
    trailSprite_.Draw();
    fillSprite_.Draw();
    flashSprite_.Draw();

    labelText_.Draw();
    valueText_.Draw();
    for (auto& popup : popups_) {
        popup.text.Draw();
    }
}

void TowerHpGauge::Reset() {
    const float ratio = target_ ? target_->GetHpRatio() : 1.0f;

    displayRatio_ = ratio;
    trailRatio_ = ratio;
    trailStartRatio_ = ratio;
    trailHoldTimer_ = 0.0f;
    trailTimer_ = 0.0f;
    flashTimer_ = 0.0f;
    punchTimer_ = 0.0f;
    shakeTimer_ = 0.0f;
    lastHp_ = target_ ? target_->GetHp() : -1.0f;

    for (auto& popup : popups_) {
        popup.active = false;
        popup.elapsed = 0.0f;
        popup.text.SetVisible(false);
    }

    ApplyGaugeSprites();
    RefreshValueText();
}

void TowerHpGauge::SetVisible(bool _visible) {
    visible_ = _visible;
    labelText_.SetVisible(_visible);
    valueText_.SetVisible(_visible);
    for (auto& popup : popups_) {
        popup.text.SetVisible(_visible && popup.active);
    }
}

void TowerHpGauge::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Tower", "MainTower")) {
        return;
    }

    const auto groups = json->GetGroups("MainTower");

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

    if (const auto gauge = groups.find("Gauge"); gauge != groups.end()) {
        gaugePosition_ = read(gauge->second, "Position", gaugePosition_);
        gaugeSize_ = read(gauge->second, "Size", gaugeSize_);
        frameThickness_ = read(gauge->second, "FrameThickness", frameThickness_);
        frameColor_ = read(gauge->second, "FrameColor", frameColor_);
        safeColor_ = read(gauge->second, "SafeColor", safeColor_);
        warningColor_ = read(gauge->second, "WarningColor", warningColor_);
        dangerColor_ = read(gauge->second, "DangerColor", dangerColor_);
        trailColor_ = read(gauge->second, "TrailColor", trailColor_);
        warningRatio_ = read(gauge->second, "WarningRatio", warningRatio_);
        dangerRatio_ = read(gauge->second, "DangerRatio", dangerRatio_);
        trailHoldSeconds_ = read(gauge->second, "TrailHoldSeconds", trailHoldSeconds_);
        trailDrainSeconds_ = read(gauge->second, "TrailDrainSeconds", trailDrainSeconds_);
    }

    if (const auto effect = groups.find("DamageEffect"); effect != groups.end()) {
        flashDuration_ = read(effect->second, "FlashDuration", flashDuration_);
        flashStrength_ = read(effect->second, "FlashStrength", flashStrength_);
        punchDuration_ = read(effect->second, "PunchDuration", punchDuration_);
        punchScale_ = read(effect->second, "PunchScale", punchScale_);
        shakeDuration_ = read(effect->second, "ShakeDuration", shakeDuration_);
        shakeStrength_ = read(effect->second, "ShakeStrength", shakeStrength_);
        shakeSpeed_ = read(effect->second, "ShakeSpeed", shakeSpeed_);
        blinkSpeed_ = read(effect->second, "DangerBlinkSpeed", blinkSpeed_);
        blinkStrength_ = read(effect->second, "DangerBlinkStrength", blinkStrength_);
        screenFlashColor_ = read(effect->second, "ScreenFlashColor", screenFlashColor_);
        screenFlashStrength_ = read(effect->second, "ScreenFlashStrength", screenFlashStrength_);
    }

    if (const auto label = groups.find("Label"); label != groups.end()) {
        label_ = read(label->second, "Text", label_);
        labelPosition_ = read(label->second, "Position", labelPosition_);
        labelFontSize_ = read(label->second, "FontSize", labelFontSize_);
        labelColor_ = read(label->second, "Color", labelColor_);
    }

    if (const auto value = groups.find("Value"); value != groups.end()) {
        valueSeparator_ = read(value->second, "Separator", valueSeparator_);
        valueRightX_ = read(value->second, "RightX", valueRightX_);
        valuePositionY_ = read(value->second, "PositionY", valuePositionY_);
        valueFontSize_ = read(value->second, "FontSize", valueFontSize_);
        valuePunchScale_ = read(value->second, "PunchScale", valuePunchScale_);
        charWidthRatio_ = read(value->second, "CharWidthRatio", charWidthRatio_);
    }

    if (const auto popup = groups.find("Popup"); popup != groups.end()) {
        popupCenterX_ = read(popup->second, "CenterX", popupCenterX_);
        popupPositionY_ = read(popup->second, "PositionY", popupPositionY_);
        popupFontSize_ = read(popup->second, "FontSize", popupFontSize_);
        popupColor_ = read(popup->second, "Color", popupColor_);
        popupDropDistance_ = read(popup->second, "DropDistance", popupDropDistance_);
        popupDuration_ = read(popup->second, "DurationSeconds", popupDuration_);
        popupStackOffset_ = read(popup->second, "StackOffset", popupStackOffset_);
    }

    // 不正な値が入っていても破綻しないように補正する
    gaugeSize_.x = std::max(gaugeSize_.x, 1.0f);
    gaugeSize_.y = std::max(gaugeSize_.y, 1.0f);
    frameThickness_ = std::max(frameThickness_, 0.0f);
    dangerRatio_ = std::clamp(dangerRatio_, 0.0f, 1.0f);
    warningRatio_ = std::clamp(warningRatio_, dangerRatio_, 1.0f);
    trailHoldSeconds_ = std::max(trailHoldSeconds_, 0.0f);
    trailDrainSeconds_ = std::max(trailDrainSeconds_, 0.0f);
    flashDuration_ = std::max(flashDuration_, 0.0f);
    flashStrength_ = std::clamp(flashStrength_, 0.0f, 1.0f);
    punchDuration_ = std::max(punchDuration_, 0.0f);
    punchScale_ = std::max(punchScale_, 1.0f);
    shakeDuration_ = std::max(shakeDuration_, 0.0f);
    shakeStrength_ = std::max(shakeStrength_, 0.0f);
    shakeSpeed_ = std::max(shakeSpeed_, 0.0f);
    blinkSpeed_ = std::max(blinkSpeed_, 0.0f);
    blinkStrength_ = std::clamp(blinkStrength_, 0.0f, 1.0f);
    screenFlashStrength_ = std::clamp(screenFlashStrength_, 0.0f, 1.0f);
    labelFontSize_ = std::max(labelFontSize_, 1.0f);
    valueFontSize_ = std::max(valueFontSize_, 1.0f);
    valuePunchScale_ = std::max(valuePunchScale_, 1.0f);
    charWidthRatio_ = std::clamp(charWidthRatio_, 0.1f, 2.0f);
    popupFontSize_ = std::max(popupFontSize_, 1.0f);
    popupDuration_ = std::max(popupDuration_, 0.01f);
    popupDropDistance_ = std::max(popupDropDistance_, 0.0f);
}

void TowerHpGauge::OnDamaged(float _damage) {
    // 周辺視野へ届く演出をまとめて起動する
    flashTimer_ = flashDuration_;   // 輝度の急変
    punchTimer_ = punchDuration_;   // 動き（縦への膨らみ）
    shakeTimer_ = shakeDuration_;   // 動き（左右の揺れ）
    SpawnDamagePopup(_damage);      // 減った量の明示

    // トレイルは「減る前の位置」から始める。本体バーが先に減るので、
    // その差分がトレイルの色で残り、どれだけ持っていかれたかが分かる。
    // 減っている最中にもう一度殴られた場合は、今のトレイル位置から引き継ぐ
    trailStartRatio_ = std::max(trailRatio_, displayRatio_);
    trailHoldTimer_ = trailHoldSeconds_;
    trailTimer_ = trailDrainSeconds_;
}

void TowerHpGauge::UpdateGauge(float _deltaTime) {
    // 本体バーは実 HP をそのまま映す。ここを遅らせると残量が読めなくなる
    displayRatio_ = target_ ? target_->GetHpRatio() : 0.0f;

    if (trailHoldTimer_ > 0.0f) {
        // 減った直後はその場に留めて「どこまで減ったか」を見せる
        trailHoldTimer_ = std::max(trailHoldTimer_ - _deltaTime, 0.0f);
        trailRatio_ = trailStartRatio_;
    } else if (trailTimer_ > 0.0f && trailDrainSeconds_ > 0.0f) {
        // 留めたあと、一定時間かけて本体バーへ追いつかせる。
        // 減少量で速さを変えず時間で制御しているので、小ダメージでも演出の長さは変わらない
        trailTimer_ = std::max(trailTimer_ - _deltaTime, 0.0f);
        const float t = 1.0f - trailTimer_ / trailDrainSeconds_;
        trailRatio_ = Lerp(trailStartRatio_, displayRatio_, EaseOutCubic(t));
    } else {
        trailTimer_ = 0.0f;
        trailRatio_ = displayRatio_;
    }

    // 回復した場合はトレイルの方が短くなるので、本体バーへ合わせ直す
    trailRatio_ = std::max(trailRatio_, displayRatio_);

    flashTimer_ = std::max(flashTimer_ - _deltaTime, 0.0f);
    punchTimer_ = std::max(punchTimer_ - _deltaTime, 0.0f);
    shakeTimer_ = std::max(shakeTimer_ - _deltaTime, 0.0f);
    blinkTime_ += _deltaTime;
}

void TowerHpGauge::UpdatePopups(float _deltaTime) {
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

        // 出現位置から下へ落とす。動き出しを速くして目の端でも動きを捉えやすくする
        popup.text.SetPosition(
            popup.baseX,
            popup.baseY + popupDropDistance_ * EaseOutCubic(t));

        // 後半に入ってから薄くしていく（前半はしっかり読ませる）
        constexpr float FADE_START = 0.55f;
        const float alpha = t < FADE_START
            ? 1.0f
            : 1.0f - (t - FADE_START) / (1.0f - FADE_START);
        popup.text.SetColor({popupColor_.x, popupColor_.y, popupColor_.z, popupColor_.w * alpha});
    }
}

void TowerHpGauge::ApplyGaugeSprites() {
    const float punch = GetPunchScale();
    const float shake = GetShakeOffset();
    const Vector4 gaugeColor = GetGaugeColor();

    // 枠は本体より frameThickness_ 分だけ外側へ広げる
    const Vector2 framePosition{gaugePosition_.x - frameThickness_ + shake, gaugePosition_.y};
    const Vector2 frameSize{
        gaugeSize_.x + frameThickness_ * 2.0f,
        (gaugeSize_.y + frameThickness_ * 2.0f) * punch};
    const Vector2 barPosition{gaugePosition_.x + shake, gaugePosition_.y};

    frameSprite_.SetPosition(framePosition);
    frameSprite_.SetSize(frameSize);
    frameSprite_.SetColor(frameColor_);
    frameSprite_.Update();

    // トレイルバーは「減る前の残量」。本体が先に減るので差分が失った量として見える。
    // 白にしているのは、本体バーが水色→橙→赤と変わってもコントラストが保たれるため
    trailSprite_.SetPosition(barPosition);
    trailSprite_.SetSize({gaugeSize_.x * trailRatio_, gaugeSize_.y * punch});
    trailSprite_.SetColor(trailColor_);
    trailSprite_.Update();

    // 本体バーは現在の残量
    fillSprite_.SetPosition(barPosition);
    fillSprite_.SetSize({gaugeSize_.x * displayRatio_, gaugeSize_.y * punch});
    fillSprite_.SetColor(gaugeColor);
    fillSprite_.Update();

    // フラッシュはゲージ全体を覆う白。輝度の急変が周辺視野に一番効く
    flashSprite_.SetPosition(framePosition);
    flashSprite_.SetSize(frameSize);
    flashSprite_.SetColor({1.0f, 1.0f, 1.0f, GetFlashAlpha()});
    flashSprite_.Update();

    // 画面全体を薄く染める。視線がゲージから離れていても被弾に気付ける
    screenFlashSprite_.SetColor({
        screenFlashColor_.x, screenFlashColor_.y, screenFlashColor_.z,
        GetFlashAlpha() * screenFlashStrength_});
    screenFlashSprite_.Update();
}

void TowerHpGauge::RefreshValueText() {
    // 表示は切り上げ。残り 0.1 でも "1" と出しておかないと
    // 「0 なのにまだ立っている」という見え方になってしまう
    const float hp = target_ ? target_->GetHp() : 0.0f;
    const float maxHp = target_ ? target_->GetMaxHp() : 0.0f;

    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%d%s%d",
                  static_cast<int32_t>(std::ceil(hp)),
                  valueSeparator_.c_str(),
                  static_cast<int32_t>(std::ceil(maxHp)));
    const std::string text = buffer;
    valueText_.SetText(text);

    // 数値もゲージと同じ色にすることで、どちらを見ても同じ状態が読み取れる
    valueText_.SetColor(GetGaugeColor());

    // 被弾した瞬間だけ数字を一回り大きくする
    const float punchT = punchDuration_ > 0.0f ? punchTimer_ / punchDuration_ : 0.0f;
    const float fontSize = valueFontSize_ * Lerp(1.0f, valuePunchScale_, EaseOutCubic(punchT));
    valueText_.SetFontSize(fontSize);

    // Text は左揃えしかできないので、幅を見積もって左へ寄せ、右端をゲージに合わせる。
    // 毎フレーム計算しているため、拡大しても右端は動かず左へ伸びる
    valueText_.SetPosition(
        valueRightX_ - EstimateTextWidth(text, fontSize),
        valuePositionY_);
}

void TowerHpGauge::SpawnDamagePopup(float _damage) {
    // 空いているポップアップを探す。全部使用中なら一番古いものを再利用する
    DamagePopup* target = nullptr;
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

    // 連続で殴られたときに文字が完全に重ならないよう、表示中の数だけ下へずらす
    int32_t activeCount = 0;
    for (const auto& popup : popups_) {
        if (popup.active && &popup != target) {
            ++activeCount;
        }
    }
    target->baseY = popupPositionY_ + static_cast<float>(activeCount) * popupStackOffset_;

    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "-%d", static_cast<int32_t>(std::ceil(_damage)));

    // 文字数で幅が変わるので、出すときに中央揃えの X を決めておく
    const std::string text = buffer;
    target->baseX = popupCenterX_ - EstimateTextWidth(text, popupFontSize_) * 0.5f;

    target->text.SetText(text);
    target->text.SetFontSize(popupFontSize_);
    target->text.SetColor(popupColor_);
    target->text.SetPosition(target->baseX, target->baseY);
    target->text.SetVisible(true);
    target->elapsed = 0.0f;
    target->active = true;
}

Vector4 TowerHpGauge::GetGaugeColor() const {
    const float ratio = displayRatio_;

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

float TowerHpGauge::GetShakeOffset() const {
    if (shakeTimer_ <= 0.0f || shakeDuration_ <= 0.0f) {
        return 0.0f;
    }

    // 振れ幅を時間とともに小さくしながら左右に振る（殴られた感触を出す）
    const float t = shakeTimer_ / shakeDuration_;
    const float elapsed = shakeDuration_ - shakeTimer_;
    return std::sin(elapsed * shakeSpeed_) * shakeStrength_ * t;
}

float TowerHpGauge::GetPunchScale() const {
    if (punchTimer_ <= 0.0f || punchDuration_ <= 0.0f) {
        return 1.0f;
    }

    // 残り時間の割合をそのまま強さに使うと「膨らんで戻る」動きになる
    const float t = punchTimer_ / punchDuration_;
    return Lerp(1.0f, punchScale_, EaseOutCubic(t));
}

float TowerHpGauge::GetFlashAlpha() const {
    if (flashTimer_ <= 0.0f || flashDuration_ <= 0.0f) {
        return 0.0f;
    }

    // 消えるときはゆるやかにして残像感を出す
    const float t = flashTimer_ / flashDuration_;
    return flashStrength_ * EaseOutCubic(t);
}

float TowerHpGauge::EstimateTextWidth(const std::string& _text, float _fontSize) const {
    // Text クラスは描画幅を返してくれないので、
    // 「フォントサイズ × 係数」を1文字分の幅とみなして概算する
    return static_cast<float>(_text.size()) * _fontSize * charWidthRatio_;
}
