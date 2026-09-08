#define NOMINMAX

#include "ResultOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <cstdio>
#include <iterator>
#include <string>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Ui/TextElement.hpp"
#include "UI/UiAnimPresets.hpp"

namespace {
    /// 読み込む Canvas 名(= Assets/Data/UI/<名前>.json)
    constexpr const char* CANVAS_NAME = "Result";

    /// 「タイトルへ戻る」のアクションキー。JSON の Events に書く文字列と一致させること
    constexpr const char* ACTION_TO_TITLE = "Result.ToTitle";

    /// 拡大縮小の中心に使う要素の名前
    constexpr const char* PANEL_NAME = "Panel";

    /// シートと一緒に動く要素。この順で JSON にも並べている
    constexpr const char* PANEL_PARTS[] = {
        "PanelBorder", "Panel", "Divider", "TitleText"
    };

    /// 成績1行を構成する要素の名前
    struct RowNames { const char* label; const char* value; };
    constexpr RowNames ROW_NAMES[] = {
        {"TimeLabel",  "TimeValue"},
        {"ScoreLabel", "ScoreValue"},
        {"TotalLabel", "TotalValue"},
    };

    /// ランキングの保存先(Assets/Data/Ranking/Ranking.json)
    constexpr const char* RANKING_DIR  = "Ranking";
    constexpr const char* RANKING_NAME = "Ranking";

    /// 今回の記録を目立たせる色
    constexpr Vector4 NEW_RECORD_COLOR{1.0f, 0.85f, 0.3f, 1.0f};

    /// 記録が無い順位に出す文字
    constexpr const char* EMPTY_RANK_TEXT = "-------";

    /// ランキング1行分の要素名を作る
    std::string RankElementName(size_t _index, const char* _suffix) {
        return "Rank" + std::to_string(_index + 1) + _suffix;
    }

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
    LoadRanking();

    // アニメーション関数とアクションは Setup(=JSON読み込み)より先に登録する。
    // 読み込み時に IdleAnim / Events のキーから解決されるため、後からでは効かない
    UiAnimPresets::RegisterAll(canvas_);
    canvas_.RegisterAction(ACTION_TO_TITLE, [this] {
        if (returnAccepting_ && onReturn_) onReturn_();
    });

    canvas_.Setup(CANVAS_NAME);

    // 土台の値を控えるのはここだけ。演出の途中経過を控えてしまうと、
    // 2回目以降の表示で暗幕の濃さなどが少しずつズレていく
    CaptureBases();

    // 読み込み直後は表示状態なので、リザルトを出すまで閉じておく
    Hide();
}

void ResultOverlay::SetOnReturn(std::function<void()> _onReturn) {
    onReturn_ = std::move(_onReturn);
}

void ResultOverlay::Update(float _deltaTime) {
    if (!active_) {
        return;
    }

    elapsed_ += std::max(_deltaTime, 0.0f);

    // 倒された瞬間はレーザーのために左クリックを押していることが多い。
    // すぐ受け付けると成績を見る前にタイトルへ飛んでしまうので、少し待つ
    returnAccepting_ = elapsed_ >= returnDelay_;

    if (!animating_) {
        return;
    }

    Apply();

    // 演出が終わったら書き戻しをやめる。以降はエディタでの変更がそのまま効く
    if (elapsed_ >= GetTotalDuration()) {
        animating_ = false;
    }
}

void ResultOverlay::Show(float _survivedSeconds, int32_t _score) {
    // 表示中に呼ばれても演出を巻き戻さない。
    // 「HPが0のあいだ毎フレーム呼ぶ」という素直な使い方をそのまま許すため
    if (active_) {
        return;
    }

    active_ = true;
    elapsed_ = 0.0f;
    returnAccepting_ = false;
    animating_ = true;
    resultSeconds_ = std::isfinite(_survivedSeconds) ? std::max(_survivedSeconds, 0.0f) : 0.0f;
    resultScore_ = _score;
    resultTotal_ = CalcTotalScore(resultSeconds_, resultScore_);

    // 今回の記録をランキングへ入れて保存する。
    // 保存はここだけなので、リザルトを見た時点で記録が残る
    InsertRanking({resultTotal_, resultScore_, resultSeconds_});
    SaveRanking();

    // 一度 Inactive を挟んで Show から始めると、カーソルの初期化とフォーカスの取得も行われる
    canvas_.SetActive(false);
    canvas_.SetActive(true);

    RefreshRankingTexts();

    // 出した最初のフレームから正しい位置に置いておく
    Apply();
}

void ResultOverlay::Hide() {
    active_ = false;
    elapsed_ = 0.0f;
    returnAccepting_ = false;
    animating_ = false;
    canvas_.SetActive(false);
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
        dimStrength_ = read(dim->second, "Strength", dimStrength_);
        dimFadeDuration_ = read(dim->second, "FadeDuration", dimFadeDuration_);
    }

    if (const auto panel = groups.find("Panel"); panel != groups.end()) {
        panelDelay_ = read(panel->second, "AppearDelay", panelDelay_);
        panelDuration_ = read(panel->second, "AppearDuration", panelDuration_);
        panelStartScale_ = read(panel->second, "StartScale", panelStartScale_);
    }

    if (const auto total = groups.find("Total"); total != groups.end()) {
        scoreWeight_ = read(total->second, "ScoreWeight", scoreWeight_);
        timeBonusPerSecond_ = read(total->second, "TimeBonusPerSecond", timeBonusPerSecond_);
        totalDigits_ = read(total->second, "Digits", totalDigits_);
    }

    if (const auto row = groups.find("Row"); row != groups.end()) {
        scoreDigits_ = read(row->second, "ScoreDigits", scoreDigits_);
    }

    if (const auto ranking = groups.find("Ranking"); ranking != groups.end()) {
        rankingInterval_ = read(ranking->second, "Interval", rankingInterval_);
    }

    if (const auto effect = groups.find("RowEffect"); effect != groups.end()) {
        rowDelay_ = read(effect->second, "Delay", rowDelay_);
        rowInterval_ = read(effect->second, "Interval", rowInterval_);
        rowFadeDuration_ = read(effect->second, "FadeDuration", rowFadeDuration_);
        rowRiseDistance_ = read(effect->second, "RiseDistance", rowRiseDistance_);
        countUpDuration_ = read(effect->second, "CountUpDuration", countUpDuration_);
    }

    if (const auto ret = groups.find("Return"); ret != groups.end()) {
        returnDelay_ = read(ret->second, "Delay", returnDelay_);
        returnFadeDuration_ = read(ret->second, "FadeDuration", returnFadeDuration_);
    }

    // 不正な値が入っていても破綻しないように補正する
    dimStrength_ = std::clamp(dimStrength_, 0.0f, 1.0f);
    dimFadeDuration_ = std::max(dimFadeDuration_, 0.0f);
    panelDelay_ = std::max(panelDelay_, 0.0f);
    panelDuration_ = std::max(panelDuration_, 0.0f);
    panelStartScale_ = std::max(panelStartScale_, 0.01f);
    rowDelay_ = std::max(rowDelay_, 0.0f);
    rowInterval_ = std::max(rowInterval_, 0.0f);
    rowFadeDuration_ = std::max(rowFadeDuration_, 0.0f);
    countUpDuration_ = std::max(countUpDuration_, 0.0f);
    scoreDigits_ = std::clamp(scoreDigits_, 0, 12);
    totalDigits_ = std::clamp(totalDigits_, 0, 12);
    scoreWeight_ = std::max(scoreWeight_, 0.0f);
    timeBonusPerSecond_ = std::max(timeBonusPerSecond_, 0);
    rankingInterval_ = std::max(rankingInterval_, 0.0f);
    returnDelay_ = std::max(returnDelay_, 0.0f);
    returnFadeDuration_ = std::max(returnFadeDuration_, 0.0f);
}

void ResultOverlay::CaptureBases() {
    bases_.clear();

    for (size_t i = 0; i < canvas_.GetElementCount(); ++i) {
        const Ui::Element* element = canvas_.GetElement(i);
        if (!element) continue;

        ElementBase base{};
        base.position = element->GetPosition();
        base.size = element->GetSize();   // Text では {0,0} が返る
        base.color = element->GetColor();
        if (element->GetType() == "Text") {
            base.fontSize = static_cast<const Ui::TextElement*>(element)->GetFontSize();
        }
        bases_[element->GetName()] = base;
    }

    // 拡大縮小の中心はシートの位置。エディタでシートを動かせば全体が付いてくる
    if (const auto panel = bases_.find(PANEL_NAME); panel != bases_.end()) {
        panelCenter_ = panel->second.position;
    }
}

void ResultOverlay::Apply() {
    // 暗幕は最初に濃くなりきる。操作が終わったことを先に伝えてから中身を見せる
    Ui::Element* dim = nullptr;
    const ElementBase* dimBase = nullptr;
    if (FindElement("Dim", dim, dimBase)) {
        const float progress = EaseOutCubic(CalcProgress(0.0f, dimFadeDuration_));
        dim->SetColor(SpriteColor(dimBase->color, dimStrength_ * progress));
    }

    // シートは少し大きい状態から等倍へ縮みながら現れる。
    // 枠・帯・線・見出しもすべて同じ倍率で扱うので、シート全体が一枚の板として動く
    const float appear = EaseOutCubic(CalcProgress(panelDelay_, panelDuration_));
    const float scale = Lerp(panelStartScale_, 1.0f, appear);

    ApplyPanel(scale, appear);
    ApplyRow(Row::Time, scale);
    ApplyRow(Row::Score, scale);
    ApplyRow(Row::Total, scale);
    ApplyRanking(scale);
    ApplyReturn(scale);
}

void ResultOverlay::ApplyPanel(float _scale, float _appear) {
    for (const char* name : PANEL_PARTS) {
        Ui::Element* element = nullptr;
        const ElementBase* base = nullptr;
        if (!FindElement(name, element, base)) continue;

        element->SetPosition(ScaledPosition(base->position, _scale));

        if (base->fontSize > 0.0f) {
            // 文字はスプライトと違って不透明度が線形に効く
            element->SetFontSize(base->fontSize * _scale);
            element->SetColor(WithAlpha(base->color, _appear));
        } else {
            element->SetSize({base->size.x * _scale, base->size.y * _scale});
            element->SetColor(SpriteColor(base->color, _appear));
        }
    }
}

void ResultOverlay::ApplyRow(Row _row, float _scale) {
    const auto index = static_cast<size_t>(_row);
    if (index >= std::size(ROW_NAMES)) return;

    const float delay = GetRowDelay(_row);
    const float reveal = EaseOutCubic(CalcProgress(delay, rowFadeDuration_));

    // 出現時はわずかに下から浮き上がらせる。動きが付くと行が増えたことに気付きやすい
    const float rise = (1.0f - reveal) * rowRiseDistance_;

    // 数字は 0 から最終値へ回していく。行ごとに開始をずらして1行ずつ読ませる
    const float count = EaseOutCubic(CalcProgress(delay, countUpDuration_));
    const auto countTo = [count](int32_t _value) {
        return static_cast<int32_t>(std::lround(static_cast<double>(_value) * count));
    };
    std::string valueText;
    switch (_row) {
    case Row::Time:  valueText = MakeTimeText(resultSeconds_ * count); break;
    case Row::Score: valueText = MakePaddedText(countTo(resultScore_), scoreDigits_); break;
    case Row::Total: valueText = MakePaddedText(countTo(resultTotal_), totalDigits_); break;
    }

    const RowNames& names = ROW_NAMES[index];
    for (int part = 0; part < 2; ++part) {
        const char* name = (part == 0) ? names.label : names.value;

        Ui::Element* element = nullptr;
        const ElementBase* base = nullptr;
        if (!FindElement(name, element, base)) continue;

        Vector2 position = ScaledPosition(base->position, _scale);
        position.y += rise;

        element->SetPosition(position);
        element->SetFontSize(base->fontSize * _scale);
        element->SetColor(WithAlpha(BaseColorOf(name, base->color), reveal));

        if (part == 1) element->SetText(valueText);
    }
}

void ResultOverlay::ApplyReturn(float _scale) {
    const float reveal = EaseOutCubic(CalcProgress(returnDelay_, returnFadeDuration_));
    const float rise = (1.0f - reveal) * rowRiseDistance_;

    for (const char* name : {"ReturnButton", "ReturnLabel"}) {
        Ui::Element* element = nullptr;
        const ElementBase* base = nullptr;
        if (!FindElement(name, element, base)) continue;

        // 出ていないボタンを押せる状態を作らない。
        // 非表示のあいだはマウスの当たり判定からも外れる
        element->SetVisible(reveal > 0.0f);
        if (reveal <= 0.0f) continue;

        Vector2 position = ScaledPosition(base->position, _scale);
        position.y += rise;
        element->SetPosition(position);

        if (base->fontSize > 0.0f) {
            element->SetFontSize(base->fontSize * _scale);
            element->SetColor(WithAlpha(base->color, reveal));
        } else {
            element->SetSize({base->size.x * _scale, base->size.y * _scale});
            element->SetColor(SpriteColor(base->color, reveal));
        }
    }
}

bool ResultOverlay::FindElement(const std::string& _name, Ui::Element*& _outElement,
                                const ElementBase*& _outBase) const {
    const auto base = bases_.find(_name);
    if (base == bases_.end()) return false;

    Ui::Element* element = canvas_.FindElementByName(_name);
    if (!element) return false;

    _outElement = element;
    _outBase = &base->second;
    return true;
}

Vector2 ResultOverlay::ScaledPosition(const Vector2& _base, float _scale) const {
    return {panelCenter_.x + (_base.x - panelCenter_.x) * _scale,
            panelCenter_.y + (_base.y - panelCenter_.y) * _scale};
}

std::string ResultOverlay::MakeTimeText(float _seconds) const {
    // 生存時間の UI と同じ "分:秒" 表記にして、プレイ中に見ていた数字と繋げる
    const int32_t total = static_cast<int32_t>(_seconds);
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
    return buffer;
}

std::string ResultOverlay::MakePaddedText(int32_t _value, int32_t _digits) const {
    std::string text = std::to_string(std::max(_value, 0));
    if (const size_t digits = static_cast<size_t>(std::max(_digits, 0)); text.size() < digits) {
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

float ResultOverlay::GetTotalDuration() const {
    const float lastRow = GetRowDelay(Row::Total)
        + std::max(rowFadeDuration_, countUpDuration_);
    const float lastRank = GetRankingDelay()
        + rankingInterval_ * static_cast<float>(RANKING_COUNT) + rowFadeDuration_;
    const float returnEnd = returnDelay_ + returnFadeDuration_;
    return std::max({dimFadeDuration_, panelDelay_ + panelDuration_, lastRow, lastRank, returnEnd});
}

float ResultOverlay::GetRankingDelay() const {
    // 左列の行がすべて出そろってから、右列のランキングへ視線を移させる
    return panelDelay_ + rowDelay_ + rowInterval_ * static_cast<float>(std::size(ROW_NAMES));
}

const Vector4& ResultOverlay::BaseColorOf(const std::string& _name, const Vector4& _base) const {
    const auto found = colorOverrides_.find(_name);
    return (found != colorOverrides_.end()) ? found->second : _base;
}

int32_t ResultOverlay::CalcTotalScore(float _seconds, int32_t _score) const {
    // 「敵を倒す」と「守り切る」の両方を評価する。
    // 敵1体が 100 点なので、既定の 100/秒 は 1秒守り切るごとに敵1体ぶんの重み付けになる
    const double fromScore = static_cast<double>(_score) * static_cast<double>(scoreWeight_);
    const double fromTime = std::floor(static_cast<double>(_seconds))
        * static_cast<double>(timeBonusPerSecond_);
    return static_cast<int32_t>(std::lround(std::max(fromScore + fromTime, 0.0)));
}

void ResultOverlay::LoadRanking() {
    ranking_.clear();
    newRankIndex_ = -1;

    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load(RANKING_DIR, RANKING_NAME)) {
        return;
    }

    // 記録が無い（total が 0）枠は読み飛ばす。初回起動でも空のランキングとして扱える
    for (const auto& [groupName, group] : json->GetGroups(RANKING_NAME)) {
        const auto readInt = [&group](const std::string& _key) {
            const auto entry = group.find(_key);
            if (entry == group.end()) return 0;
            if (const auto value = std::get_if<int32_t>(&entry->second)) return *value;
            if (const auto value = std::get_if<float>(&entry->second)) {
                return static_cast<int32_t>(*value);
            }
            return 0;
        };
        const auto readFloat = [&group](const std::string& _key) {
            const auto entry = group.find(_key);
            if (entry == group.end()) return 0.0f;
            if (const auto value = std::get_if<float>(&entry->second)) return *value;
            if (const auto value = std::get_if<int32_t>(&entry->second)) {
                return static_cast<float>(*value);
            }
            return 0.0f;
        };

        RankingEntry record{};
        record.total = readInt("Total");
        record.score = readInt("Score");
        record.seconds = readFloat("Seconds");
        if (record.total > 0) ranking_.push_back(record);
    }

    std::ranges::sort(ranking_, std::ranges::greater{}, &RankingEntry::total);
    if (ranking_.size() > RANKING_COUNT) ranking_.resize(RANKING_COUNT);
}

void ResultOverlay::SaveRanking() const {
    const auto json = Singleton<JsonParams>::GetInstance();

    // 空き枠も 0 で書いておく。次の起動で必ず読み込めるようにするため
    for (size_t i = 0; i < RANKING_COUNT; ++i) {
        const std::string group = "Entry" + std::to_string(i);
        const RankingEntry record = (i < ranking_.size()) ? ranking_[i] : RankingEntry{};
        json->SetValue(RANKING_NAME, group, "Total", record.total);
        json->SetValue(RANKING_NAME, group, "Score", record.score);
        json->SetValue(RANKING_NAME, group, "Seconds", record.seconds);
    }

    // @note Save() は書き出したあとに内部の保持データを捨てる。
    //       読み直しはせず、ranking_ を正としてこちらで持ち続ける
    json->Save(RANKING_DIR, RANKING_NAME);
}

void ResultOverlay::InsertRanking(const RankingEntry& _entry) {
    newRankIndex_ = -1;
    if (_entry.total <= 0) return;

    // 同点なら先に載っている記録を上に残す（後から並んだ記録が抜かさない）
    const auto position = std::ranges::find_if(ranking_,
        [&_entry](const RankingEntry& _record) { return _entry.total > _record.total; });

    const auto index = static_cast<size_t>(std::distance(ranking_.begin(), position));
    if (index >= RANKING_COUNT) return; // ランク外

    ranking_.insert(position, _entry);
    if (ranking_.size() > RANKING_COUNT) ranking_.resize(RANKING_COUNT);
    newRankIndex_ = static_cast<int32_t>(index);
}

void ResultOverlay::RefreshRankingTexts() {
    colorOverrides_.clear();

    for (size_t i = 0; i < RANKING_COUNT; ++i) {
        const std::string scoreName = RankElementName(i, "Score");
        if (Ui::Element* element = canvas_.FindElementByName(scoreName)) {
            element->SetText(i < ranking_.size()
                ? MakePaddedText(ranking_[i].total, totalDigits_)
                : EMPTY_RANK_TEXT);
        }

        // 今回の記録だけ色を変えて、どれが自分の記録か一目で分かるようにする
        if (static_cast<int32_t>(i) != newRankIndex_) continue;
        colorOverrides_[RankElementName(i, "No")] = NEW_RECORD_COLOR;
        colorOverrides_[scoreName] = NEW_RECORD_COLOR;
    }
}

void ResultOverlay::ApplyRanking(float _scale) {
    const float titleDelay = GetRankingDelay();

    // 見出し → 1位 → 2位 … の順に出す
    for (size_t i = 0; i <= RANKING_COUNT; ++i) {
        const float delay = titleDelay + rankingInterval_ * static_cast<float>(i);
        const float reveal = EaseOutCubic(CalcProgress(delay, rowFadeDuration_));
        const float rise = (1.0f - reveal) * rowRiseDistance_;

        const bool isTitle = (i == 0);
        const size_t rank = i - 1;
        const std::string names[] = {
            isTitle ? std::string("RankingTitle") : RankElementName(rank, "No"),
            isTitle ? std::string() : RankElementName(rank, "Score"),
        };

        for (const std::string& name : names) {
            if (name.empty()) continue;

            Ui::Element* element = nullptr;
            const ElementBase* base = nullptr;
            if (!FindElement(name, element, base)) continue;

            Vector2 position = ScaledPosition(base->position, _scale);
            position.y += rise;
            element->SetPosition(position);
            element->SetFontSize(base->fontSize * _scale);
            element->SetColor(WithAlpha(BaseColorOf(name, base->color), reveal));
        }
    }
}
