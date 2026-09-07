#define NOMINMAX

#include "SurvivalTimeManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// 目盛りに使う単色テクスチャ
    const std::string WHITE_TEXTURE = "white_x16.png";

    /// 円周を一周するラジアン
    constexpr float TWO_PI = 6.283185307f;

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

void SurvivalTimeManager::Initialize() {
    LoadConfig();

    elapsedSeconds_ = 0.0f;
    lapCount_ = 0;
    lapFlashTimer_ = 0.0f;
    punchTimer_ = 0.0f;
    tickGainTimers_.fill(0.0f);
    litCount_ = CalcLitCount();

    // 目盛りは単色テクスチャを回転させて作る。
    // アンカーは中央（既定）のままにして、目盛りの中心を円周上へ置く
    for (int32_t i = 0; i < tickCount_; ++i) {
        tickSprites_[static_cast<size_t>(i)].Initialize(WHITE_TEXTURE);
    }

    valueText_.Initialize("", ringCenter_.x, ringCenter_.y + valueOffsetY_, valueFontSize_);
    valueText_.SetColor(valueColor_);

    labelText_.Initialize(label_, ringCenter_.x, ringCenter_.y + labelOffsetY_, labelFontSize_);
    labelText_.SetColor(labelColor_);
    // ラベルは長さが変わらないので、位置は一度決めれば動かさなくてよい
    labelText_.SetPosition(
        ringCenter_.x - EstimateTextWidth(label_, labelFontSize_) * 0.5f,
        ringCenter_.y + labelOffsetY_);

    ApplyTickSprites();
    RefreshValueText();
}

void SurvivalTimeManager::Update(float _deltaTime) {
    if (counting_) {
        elapsedSeconds_ += _deltaTime;
    }

    UpdateRing(_deltaTime);
    ApplyTickSprites();
    RefreshValueText();
}

void SurvivalTimeManager::Draw() {
    if (!visible_) {
        return;
    }

    for (int32_t i = 0; i < tickCount_; ++i) {
        tickSprites_[static_cast<size_t>(i)].Draw();
    }

    valueText_.Draw();
    labelText_.Draw();
}

void SurvivalTimeManager::Reset() {
    elapsedSeconds_ = 0.0f;
    lapCount_ = 0;
    lapFlashTimer_ = 0.0f;
    punchTimer_ = 0.0f;
    tickGainTimers_.fill(0.0f);
    litCount_ = CalcLitCount();

    ApplyTickSprites();
    RefreshValueText();
}

void SurvivalTimeManager::SetVisible(bool _visible) {
    visible_ = _visible;
    valueText_.SetVisible(_visible);
    labelText_.SetVisible(_visible);
}

void SurvivalTimeManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("SurvivalTime", "SurvivalTime")) {
        return;
    }

    const auto groups = json->GetGroups("SurvivalTime");

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

    if (const auto ring = groups.find("Ring"); ring != groups.end()) {
        ringCenter_ = read(ring->second, "Center", ringCenter_);
        ringRadius_ = read(ring->second, "Radius", ringRadius_);
        tickSize_ = read(ring->second, "TickSize", tickSize_);
        tickCount_ = read(ring->second, "TickCount", tickCount_);
        secondsPerLap_ = read(ring->second, "SecondsPerLap", secondsPerLap_);
        emptyColor_ = read(ring->second, "EmptyColor", emptyColor_);
        gainColor_ = read(ring->second, "GainColor", gainColor_);

        // 周回色は [[r,g,b], [r,g,b], ...] の形で持つ（不透明度は常に1）
        if (const auto colors = read(ring->second, "LapColors", std::vector<Vector3>{});
            !colors.empty()) {
            lapColors_.clear();
            for (const auto& color : colors) {
                lapColors_.push_back({color.x, color.y, color.z, 1.0f});
            }
        }
        gainFlashDuration_ = read(ring->second, "GainFlashDuration", gainFlashDuration_);
    }

    if (const auto effect = groups.find("LapEffect"); effect != groups.end()) {
        lapFlashDuration_ = read(effect->second, "FlashDuration", lapFlashDuration_);
        punchDuration_ = read(effect->second, "PunchDuration", punchDuration_);
        punchScale_ = read(effect->second, "PunchScale", punchScale_);
        valuePunchScale_ = read(effect->second, "ValuePunchScale", valuePunchScale_);
    }

    if (const auto value = groups.find("Value"); value != groups.end()) {
        valueFontSize_ = read(value->second, "FontSize", valueFontSize_);
        minuteDigits_ = read(value->second, "MinuteDigits", minuteDigits_);
        valueOffsetY_ = read(value->second, "OffsetY", valueOffsetY_);
        valueColor_ = read(value->second, "Color", valueColor_);
        charWidthRatio_ = read(value->second, "CharWidthRatio", charWidthRatio_);
    }

    if (const auto label = groups.find("Label"); label != groups.end()) {
        label_ = read(label->second, "Text", label_);
        labelFontSize_ = read(label->second, "FontSize", labelFontSize_);
        labelOffsetY_ = read(label->second, "OffsetY", labelOffsetY_);
        labelColor_ = read(label->second, "Color", labelColor_);
    }

    // 不正な値が入っていても破綻しないように補正する
    tickCount_ = std::clamp(tickCount_, 4, static_cast<int32_t>(TICK_MAX));
    secondsPerLap_ = std::max(secondsPerLap_, 1.0f);
    ringRadius_ = std::max(ringRadius_, 1.0f);
    tickSize_.x = std::max(tickSize_.x, 1.0f);
    tickSize_.y = std::max(tickSize_.y, 1.0f);
    gainFlashDuration_ = std::max(gainFlashDuration_, 0.0f);
    lapFlashDuration_ = std::max(lapFlashDuration_, 0.0f);
    punchDuration_ = std::max(punchDuration_, 0.0f);
    punchScale_ = std::max(punchScale_, 1.0f);
    valuePunchScale_ = std::max(valuePunchScale_, 1.0f);
    valueFontSize_ = std::max(valueFontSize_, 1.0f);
    minuteDigits_ = std::clamp(minuteDigits_, 1, 4);
    labelFontSize_ = std::max(labelFontSize_, 1.0f);
    charWidthRatio_ = std::clamp(charWidthRatio_, 0.1f, 2.0f);
    if (lapColors_.empty()) {
        lapColors_.push_back({0.25f, 0.95f, 0.55f, 1.0f});
    }
}

void SurvivalTimeManager::UpdateRing(float _deltaTime) {
    // 何周したかは経過時間から直に求める。デルタの積み上げで数えると
    // フレーム落ちで周回を取りこぼすことがある
    const int32_t lap = static_cast<int32_t>(elapsedSeconds_ / secondsPerLap_);
    const bool lapped = lap > lapCount_;
    lapCount_ = lap;

    const int32_t before = litCount_;
    litCount_ = CalcLitCount();

    if (lapped) {
        // 1分耐えきった合図。リング全体を光らせて空へ戻す
        lapFlashTimer_ = lapFlashDuration_;
        punchTimer_ = punchDuration_;
        tickGainTimers_.fill(0.0f);
    } else {
        // 灯ったばかりの目盛りだけを白く光らせる。
        // 3秒ごとに1つ光るので、視線を向けていなくても時間が進んでいるのが分かる
        for (int32_t i = before; i < litCount_; ++i) {
            tickGainTimers_[static_cast<size_t>(i)] = gainFlashDuration_;
        }
    }

    for (auto& timer : tickGainTimers_) {
        timer = std::max(timer - _deltaTime, 0.0f);
    }

    lapFlashTimer_ = std::max(lapFlashTimer_ - _deltaTime, 0.0f);
    punchTimer_ = std::max(punchTimer_ - _deltaTime, 0.0f);
}

void SurvivalTimeManager::ApplyTickSprites() {
    const float punch = GetPunchScale();
    const float lapFlash = GetLapFlashAlpha();
    const float step = TWO_PI / static_cast<float>(tickCount_);

    for (int32_t i = 0; i < tickCount_; ++i) {
        auto& sprite = tickSprites_[static_cast<size_t>(i)];

        // 12時の位置から時計回りに並べる。画面のY軸は下向きなので cos を引く
        const float angle = step * static_cast<float>(i);
        sprite.SetPosition({
            ringCenter_.x + std::sin(angle) * ringRadius_,
            ringCenter_.y - std::cos(angle) * ringRadius_});
        // 目盛りの長辺が中心から外を向くように、並べた角度と同じだけ回す
        sprite.SetRotation(angle);

        const bool lit = i < litCount_;

        // 1周した瞬間は全部の目盛りが伸びる（点灯中はさらに強調される）
        const float length = tickSize_.y * (lit || lapFlash > 0.0f ? punch : 1.0f);
        sprite.SetSize({tickSize_.x, length});

        // 今の周回の色を先頭から重ねていき、まだ届いていない部分には
        // 1つ前の周回の色を残す。こうするとリングは空に戻らず満タンのまま色だけが変わる
        const Vector4 filled = lapCount_ > 0 ? GetLapColor(lapCount_ - 1) : emptyColor_;
        const Vector4 base = lit ? GetLapColor(lapCount_) : filled;

        // 灯ったばかりの目盛り、および1周した瞬間の全目盛りを白く光らせる
        const float gain = gainFlashDuration_ > 0.0f
            ? tickGainTimers_[static_cast<size_t>(i)] / gainFlashDuration_
            : 0.0f;
        const float highlight = std::max(EaseOutCubic(std::clamp(gain, 0.0f, 1.0f)), lapFlash);
        sprite.SetColor(LerpColor(base, gainColor_, highlight));

        sprite.Update();
    }
}

void SurvivalTimeManager::RefreshValueText() {
    // 通算の生存時間を "分:秒" で出す。リングが今の1分を表しているので、
    // 数値と合わせて「何分何秒耐えたか」がひと目で読める
    const int32_t total = static_cast<int32_t>(elapsedSeconds_);
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
    const std::string text = buffer;
    valueText_.SetText(text);

    // 1周した瞬間だけ数値を白く光らせて一回り大きくする
    const float lapFlash = GetLapFlashAlpha();
    valueText_.SetColor(LerpColor(valueColor_, gainColor_, lapFlash));

    const float punchT = punchDuration_ > 0.0f ? punchTimer_ / punchDuration_ : 0.0f;
    const float fontSize = valueFontSize_ * Lerp(1.0f, valuePunchScale_, EaseOutCubic(punchT));
    valueText_.SetFontSize(fontSize);

    // 分は minuteDigits_ 桁ぶんの場所を常に確保しておく。
    // 1桁のうちは十の位のぶんだけ右へずらして描くので、10分になって桁が増えても
    // ":" から右の位置が動かない。
    // 空白文字を頭に足す方法だと、空白の送り幅が数字と違うぶんだけずれてしまうため、
    // 文字を足すのではなく描き始めの位置でスペースを作っている
    const float digitWidth = fontSize * charWidthRatio_;
    const int32_t writtenDigits = static_cast<int32_t>(text.find(':'));
    const float pad = static_cast<float>(std::max(minuteDigits_ - writtenDigits, 0)) * digitWidth;
    // "MM:SS" のように、確保した分の桁 + ":" + 秒2桁ぶんを表示領域の幅とみなす
    const float fieldWidth = static_cast<float>(minuteDigits_ + 3) * digitWidth;

    valueText_.SetPosition(
        ringCenter_.x - fieldWidth * 0.5f + pad,
        ringCenter_.y + valueOffsetY_);
}

int32_t SurvivalTimeManager::CalcLitCount() const {
    // 1目盛り = secondsPerLap_ / tickCount_ 秒。
    // 今の周回に入ってからの経過分だけを数えるので、1周ごとに空へ戻る
    const float perTick = secondsPerLap_ / static_cast<float>(tickCount_);
    if (perTick <= 0.0f) {
        return 0;
    }
    const float inLap = std::fmod(elapsedSeconds_, secondsPerLap_);
    const int32_t lit = static_cast<int32_t>(inLap / perTick);
    return std::clamp(lit, 0, tickCount_);
}

Vector4 SurvivalTimeManager::GetLapColor(int32_t _lap) const {
    // 用意した色を使い切ったら先頭へ戻る。長く耐えるほど色が一巡していく
    const int32_t count = static_cast<int32_t>(lapColors_.size());
    if (count <= 0) {
        return gainColor_;
    }
    return lapColors_[static_cast<size_t>(std::max(_lap, 0) % count)];
}

float SurvivalTimeManager::GetPunchScale() const {
    if (punchTimer_ <= 0.0f || punchDuration_ <= 0.0f) {
        return 1.0f;
    }

    // 残り時間の割合をそのまま強さに使うと「膨らんで戻る」動きになる
    const float t = punchTimer_ / punchDuration_;
    return Lerp(1.0f, punchScale_, EaseOutCubic(t));
}

float SurvivalTimeManager::GetLapFlashAlpha() const {
    if (lapFlashTimer_ <= 0.0f || lapFlashDuration_ <= 0.0f) {
        return 0.0f;
    }

    // 消えるときはゆるやかにして残像感を出す
    const float t = lapFlashTimer_ / lapFlashDuration_;
    return EaseOutCubic(t);
}

float SurvivalTimeManager::EstimateTextWidth(const std::string& _text, float _fontSize) const {
    // Text クラスは描画幅を返してくれないので、
    // 「フォントサイズ × 係数」を1文字分の幅とみなして概算する
    return static_cast<float>(_text.size()) * _fontSize * charWidthRatio_;
}
