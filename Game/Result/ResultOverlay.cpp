#define NOMINMAX

#include "ResultOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"

namespace {
    /// 暗幕やシートに使う単色テクスチャ
    const std::string WHITE_TEXTURE = "white_x16.png";

    /// テキスト上端からベースラインまでの距離 ÷ フォントサイズ
    /// 大きさの違うラベルと数値の下端を揃えるのに使う
    constexpr float GLYPH_BASELINE_RATIO = 0.91f;

    /// 終わり際がゆっくりになる補間（演出の減衰に使う）
    float EaseOutCubic(float _t) {
        const float inv = 1.0f - _t;
        return 1.0f - inv * inv * inv;
    }

    /// float の線形補間
    float Lerp(float _start, float _end, float _t) {
        return _start + (_end - _start) * _t;
    }

    /// 不透明度だけを掛けた色を返す
    Vector4 WithAlpha(const Vector4& _color, float _alpha) {
        return {_color.x, _color.y, _color.z, _color.w * _alpha};
    }

    /// スプライトの不透明度の効き方を実測して合わせた指数
    /// @note このエンジンのスプライト合成は不透明度が線形に効かず、0.72 を指定しても
    ///       実際に隠れるのは 1/4 ほどしかない。指定した値をそのままの濃さで出すために、
    ///       逆側のカーブを通してから渡す。こうすると JSON の数値が見た目と一致する
    constexpr float SPRITE_ALPHA_CURVE = 4.17f;

    /// 見た目の濃さを指定して、スプライトへ渡す色を作る
    Vector4 SpriteColor(const Vector4& _color, float _alpha) {
        const float opacity = std::clamp(_color.w * _alpha, 0.0f, 1.0f);
        return {_color.x, _color.y, _color.z, std::pow(opacity, 1.0f / SPRITE_ALPHA_CURVE)};
    }
}

void ResultOverlay::Initialize() {
    LoadConfig();

    // 暗幕だけは画面全体を覆うので、左上を基準にして画面サイズまで引き伸ばす
    const auto screen = Singleton<Screen>::GetInstance();
    dimSprite_.Initialize(WHITE_TEXTURE);
    dimSprite_.SetAnchorPoint({0.0f, 0.0f});
    dimSprite_.SetPosition({0.0f, 0.0f});
    dimSprite_.SetSize({screen->Width(), screen->Height()});

    // シート側は着地演出で拡大縮小するので、中心を基準にして大きさだけを変える
    for (Sprite* sprite : {&panelBorderSprite_, &panelSprite_, &accentSprite_, &dividerSprite_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.5f, 0.5f});
        sprite->SetPosition(panelCenter_);
    }

    titleText_.Initialize(title_, panelCenter_.x, panelCenter_.y, titleFontSize_);
    titleText_.SetColor(titleColor_);

    for (Text* text : {&timeLabelText_, &scoreLabelText_}) {
        text->Initialize("", panelCenter_.x, panelCenter_.y, labelFontSize_);
        text->SetColor(labelColor_);
    }
    for (Text* text : {&timeValueText_, &scoreValueText_}) {
        text->Initialize("", panelCenter_.x, panelCenter_.y, valueFontSize_);
        text->SetColor(valueColor_);
    }

    timeLabelText_.SetText(timeLabel_);
    scoreLabelText_.SetText(scoreLabel_);

    Hide();
}

void ResultOverlay::Update(float _deltaTime) {
    if (!active_) {
        return;
    }

    elapsed_ += std::max(_deltaTime, 0.0f);

    ApplySprites();
    RefreshTexts();
}

void ResultOverlay::Draw() {
    if (!active_) {
        return;
    }

    // 暗幕 → 縁取り → 地 → 帯 → 線 の順に重ねる。
    // 暗幕を最初に描くので、ゲーム中の UI もまとめてこの下へ沈む
    dimSprite_.Draw();
    panelBorderSprite_.Draw();
    panelSprite_.Draw();
    accentSprite_.Draw();
    dividerSprite_.Draw();

    titleText_.Draw();
    timeLabelText_.Draw();
    timeValueText_.Draw();
    scoreLabelText_.Draw();
    scoreValueText_.Draw();
}

void ResultOverlay::Show(float _survivedSeconds, int32_t _score) {
    // 表示中に呼ばれても演出を巻き戻さない。
    // 「HPが0のあいだ毎フレーム呼ぶ」という素直な使い方をそのまま許すため
    if (active_) {
        return;
    }

    active_ = true;
    elapsed_ = 0.0f;
    resultSeconds_ = std::isfinite(_survivedSeconds) ? std::max(_survivedSeconds, 0.0f) : 0.0f;
    resultScore_ = _score;

    // 出した最初のフレームから正しい位置に置いておく
    ApplySprites();
    RefreshTexts();
}

void ResultOverlay::Hide() {
    active_ = false;
    elapsed_ = 0.0f;
}

void ResultOverlay::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Result", "Result")) {
        return;
    }

    const auto groups = json->GetGroups("Result");

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

    if (const auto dim = groups.find("Dim"); dim != groups.end()) {
        dimColor_ = read(dim->second, "Color", dimColor_);
        dimStrength_ = read(dim->second, "Strength", dimStrength_);
        dimFadeDuration_ = read(dim->second, "FadeDuration", dimFadeDuration_);
    }

    if (const auto panel = groups.find("Panel"); panel != groups.end()) {
        panelCenter_ = read(panel->second, "Center", panelCenter_);
        panelSize_ = read(panel->second, "Size", panelSize_);
        panelColor_ = read(panel->second, "Color", panelColor_);
        panelBorder_ = read(panel->second, "BorderThickness", panelBorder_);
        panelBorderColor_ = read(panel->second, "BorderColor", panelBorderColor_);
        accentHeight_ = read(panel->second, "AccentHeight", accentHeight_);
        accentColor_ = read(panel->second, "AccentColor", accentColor_);
        panelDelay_ = read(panel->second, "AppearDelay", panelDelay_);
        panelDuration_ = read(panel->second, "AppearDuration", panelDuration_);
        panelStartScale_ = read(panel->second, "StartScale", panelStartScale_);
    }

    if (const auto title = groups.find("Title"); title != groups.end()) {
        title_ = read(title->second, "Text", title_);
        titleFontSize_ = read(title->second, "FontSize", titleFontSize_);
        titleOffsetY_ = read(title->second, "OffsetY", titleOffsetY_);
        titleColor_ = read(title->second, "Color", titleColor_);
    }

    if (const auto divider = groups.find("Divider"); divider != groups.end()) {
        dividerOffsetY_ = read(divider->second, "OffsetY", dividerOffsetY_);
        dividerSize_ = read(divider->second, "Size", dividerSize_);
        dividerColor_ = read(divider->second, "Color", dividerColor_);
    }

    if (const auto row = groups.find("Row"); row != groups.end()) {
        rowFirstOffsetY_ = read(row->second, "FirstOffsetY", rowFirstOffsetY_);
        rowSpacing_ = read(row->second, "Spacing", rowSpacing_);
        labelOffsetX_ = read(row->second, "LabelOffsetX", labelOffsetX_);
        valueOffsetX_ = read(row->second, "ValueOffsetX", valueOffsetX_);
        labelFontSize_ = read(row->second, "LabelFontSize", labelFontSize_);
        labelColor_ = read(row->second, "LabelColor", labelColor_);
        valueFontSize_ = read(row->second, "ValueFontSize", valueFontSize_);
        valueColor_ = read(row->second, "ValueColor", valueColor_);
        timeLabel_ = read(row->second, "TimeLabel", timeLabel_);
        scoreLabel_ = read(row->second, "ScoreLabel", scoreLabel_);
        scoreDigits_ = read(row->second, "ScoreDigits", scoreDigits_);
        charWidthRatio_ = read(row->second, "CharWidthRatio", charWidthRatio_);
    }

    if (const auto effect = groups.find("RowEffect"); effect != groups.end()) {
        rowDelay_ = read(effect->second, "Delay", rowDelay_);
        rowInterval_ = read(effect->second, "Interval", rowInterval_);
        rowFadeDuration_ = read(effect->second, "FadeDuration", rowFadeDuration_);
        rowRiseDistance_ = read(effect->second, "RiseDistance", rowRiseDistance_);
        countUpDuration_ = read(effect->second, "CountUpDuration", countUpDuration_);
    }

    // 不正な値が入っていても破綻しないように補正する
    dimStrength_ = std::clamp(dimStrength_, 0.0f, 1.0f);
    dimFadeDuration_ = std::max(dimFadeDuration_, 0.0f);
    panelSize_.x = std::max(panelSize_.x, 1.0f);
    panelSize_.y = std::max(panelSize_.y, 1.0f);
    panelBorder_ = std::max(panelBorder_, 0.0f);
    accentHeight_ = std::max(accentHeight_, 0.0f);
    panelDelay_ = std::max(panelDelay_, 0.0f);
    panelDuration_ = std::max(panelDuration_, 0.0f);
    panelStartScale_ = std::max(panelStartScale_, 0.01f);
    titleFontSize_ = std::max(titleFontSize_, 1.0f);
    dividerSize_.x = std::max(dividerSize_.x, 0.0f);
    dividerSize_.y = std::max(dividerSize_.y, 0.0f);
    rowSpacing_ = std::max(rowSpacing_, 0.0f);
    labelFontSize_ = std::max(labelFontSize_, 1.0f);
    valueFontSize_ = std::max(valueFontSize_, 1.0f);
    scoreDigits_ = std::clamp(scoreDigits_, 0, 12);
    charWidthRatio_ = std::clamp(charWidthRatio_, 0.1f, 2.0f);
    rowDelay_ = std::max(rowDelay_, 0.0f);
    rowInterval_ = std::max(rowInterval_, 0.0f);
    rowFadeDuration_ = std::max(rowFadeDuration_, 0.0f);
    countUpDuration_ = std::max(countUpDuration_, 0.0f);
}

void ResultOverlay::ApplySprites() {
    // 暗幕は最初に濃くなりきる。操作が終わったことを先に伝えてから中身を見せる
    const float dim = EaseOutCubic(CalcProgress(0.0f, dimFadeDuration_));
    dimSprite_.SetColor(SpriteColor(dimColor_, dimStrength_ * dim));
    dimSprite_.Update();

    // シートは少し大きい状態から等倍へ縮みながら現れる。
    // 縁取り・帯・線・文字もすべて同じ倍率で扱うので、シート全体が一枚の板として動く
    const float appear = EaseOutCubic(CalcProgress(panelDelay_, panelDuration_));
    const float scale = Lerp(panelStartScale_, 1.0f, appear);
    const Vector2 size{panelSize_.x * scale, panelSize_.y * scale};

    panelBorderSprite_.SetPosition(panelCenter_);
    panelBorderSprite_.SetSize({size.x + panelBorder_ * 2.0f, size.y + panelBorder_ * 2.0f});
    panelBorderSprite_.SetColor(SpriteColor(panelBorderColor_, appear));
    panelBorderSprite_.Update();

    panelSprite_.SetPosition(panelCenter_);
    panelSprite_.SetSize(size);
    panelSprite_.SetColor(SpriteColor(panelColor_, appear));
    panelSprite_.Update();

    // アクセント帯はシートの上端に貼り付ける
    accentSprite_.SetPosition({panelCenter_.x,
                               panelCenter_.y - size.y * 0.5f + accentHeight_ * scale * 0.5f});
    accentSprite_.SetSize({size.x, accentHeight_ * scale});
    accentSprite_.SetColor(SpriteColor(accentColor_, appear));
    accentSprite_.Update();

    // 区切り線は見出しと成績の境目。見出しが出そろってから引く
    dividerSprite_.SetPosition({panelCenter_.x, panelCenter_.y + dividerOffsetY_ * scale});
    dividerSprite_.SetSize({dividerSize_.x * scale, dividerSize_.y * scale});
    dividerSprite_.SetColor(SpriteColor(dividerColor_, appear));
    dividerSprite_.Update();
}

void ResultOverlay::RefreshTexts() {
    const float appear = EaseOutCubic(CalcProgress(panelDelay_, panelDuration_));
    const float scale = Lerp(panelStartScale_, 1.0f, appear);

    // 見出しはシートの中央上。中央揃えなので毎フレーム幅を見積もって左へ寄せる
    const float titleSize = titleFontSize_ * scale;
    titleText_.SetFontSize(titleSize);
    titleText_.SetPosition(panelCenter_.x - EstimateTextWidth(title_, titleSize) * 0.5f,
                           panelCenter_.y + titleOffsetY_ * scale);
    titleText_.SetColor(WithAlpha(titleColor_, appear));

    // 数字は 0 から最終値へ回していく。行ごとに開始をずらして1行ずつ読ませる
    const float timeCount = EaseOutCubic(CalcProgress(GetRowDelay(Row::Time), countUpDuration_));
    const float scoreCount = EaseOutCubic(CalcProgress(GetRowDelay(Row::Score), countUpDuration_));

    ApplyRow(timeLabelText_, timeValueText_,
             MakeTimeText(resultSeconds_ * timeCount), Row::Time, scale);
    ApplyRow(scoreLabelText_, scoreValueText_,
             MakeScoreText(static_cast<int32_t>(std::lround(
                 static_cast<double>(resultScore_) * scoreCount))), Row::Score, scale);
}

void ResultOverlay::ApplyRow(Text& _label, Text& _value, const std::string& _valueText,
                             Row _row, float _scale) {
    const float delay = GetRowDelay(_row);
    const float reveal = EaseOutCubic(CalcProgress(delay, rowFadeDuration_));

    // 出現時はわずかに下から浮き上がらせる。動きが付くと行が増えたことに気付きやすい
    const float rise = (1.0f - reveal) * rowRiseDistance_;
    const float index = static_cast<float>(static_cast<int32_t>(_row));
    const float labelTop = panelCenter_.y
        + (rowFirstOffsetY_ + rowSpacing_ * index) * _scale + rise;

    const float labelSize = labelFontSize_ * _scale;
    const float valueSize = valueFontSize_ * _scale;

    // ラベルと数値はそれぞれ決まった列に左揃えで置く。
    // 大きさが違うので、上端ではなく文字の下端（ベースライン）が揃うように数値をずらす
    const float valueTop = labelTop + (labelFontSize_ - valueFontSize_) * GLYPH_BASELINE_RATIO * _scale;

    _label.SetFontSize(labelSize);
    _label.SetPosition(panelCenter_.x + labelOffsetX_ * _scale, labelTop);
    _label.SetColor(WithAlpha(labelColor_, reveal));

    _value.SetText(_valueText);
    _value.SetFontSize(valueSize);
    _value.SetPosition(panelCenter_.x + valueOffsetX_ * _scale, valueTop);
    _value.SetColor(WithAlpha(valueColor_, reveal));
}

std::string ResultOverlay::MakeTimeText(float _seconds) const {
    // 生存時間の UI と同じ "分:秒" 表記にして、プレイ中に見ていた数字と繋げる
    const int32_t total = static_cast<int32_t>(_seconds);
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
    return buffer;
}

std::string ResultOverlay::MakeScoreText(int32_t _score) const {
    std::string text = std::to_string(std::max(_score, 0));
    // 0埋めして桁数を固定する。カウントアップ中に文字数が変わって位置が動くのを防ぐ
    if (const size_t digits = static_cast<size_t>(scoreDigits_); text.size() < digits) {
        text.insert(0, digits - text.size(), '0');
    }
    return text;
}

float ResultOverlay::CalcProgress(float _delay, float _duration) const {
    if (_duration <= 0.0f) {
        return elapsed_ >= _delay ? 1.0f : 0.0f;
    }
    return std::clamp((elapsed_ - _delay) / _duration, 0.0f, 1.0f);
}

float ResultOverlay::GetRowDelay(Row _row) const {
    const float index = static_cast<float>(static_cast<int32_t>(_row));
    return panelDelay_ + rowDelay_ + rowInterval_ * index;
}

float ResultOverlay::EstimateTextWidth(const std::string& _text, float _fontSize) const {
    return static_cast<float>(_text.size()) * _fontSize * charWidthRatio_;
}
