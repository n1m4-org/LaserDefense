#ifndef RESULT_OVERLAY_HPP_
#define RESULT_OVERLAY_HPP_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Ui/UserInterface.hpp"

/** タワーが落ちたときに成績を見せるリザルトシート
 *
 *  ## なぜシーンを切り替えないのか
 *  リザルト専用シーンへ飛ぶと、プレイ中の画面が一度消えてしまい
 *  「どこで負けたのか」がプレイヤーの記憶からも画面からも失われる。
 *  そのため PlayScene のまま画面全体を暗幕（dim）で覆い、その上に
 *  シートだけを重ねている。うっすら透けるゲーム画面が最後の状況を残す。
 *
 *  ## 見せる順番
 *  一度に全部出すと数字が読み飛ばされるので、時間差で出している。
 *    1. 暗幕がフェードイン        … 操作が終わったことを伝える
 *    2. シートが少し縮みながら着地 … 視線をシートの中央へ集める
 *    3. TIME → SCORE の順に出現   … 1行ずつ読ませる
 *    4. 数字がカウントアップ       … 成績が積み上がる手応えを出す
 *    5. 「タイトルへ戻る」が出現   … 読み終わってから操作を受け付ける
 *
 *  ## 見た目と演出の分担
 *  - 配置・大きさ・色・文字は Assets/Data/UI/Result.json（UIエディタで編集できる）
 *  - 演出の速さ・間は Assets/Data/Result/Result.json
 *
 *  演出は「エディタが持つ値 × 進捗」を毎フレーム要素へ書き戻す形で作っている。
 *  Canvas のアニメーション機能（AnimFunc）は位置と色しか動かせず、
 *  シートの拡大縮小や行ごとの時間差を表現できないため、そこだけこちらで持つ。
 *  演出が終わったら書き戻しをやめるので、以降はエディタでの変更がそのまま効く。
 */
class ResultOverlay final {
    /// 成績1行（ラベルと数値の組）の識別子。表示順もこの並びになる
    enum class Row : int32_t {
        Time = 0,
        Score = 1,
        Total = 2,
    };

public:
    /// ランキングに載る1回分の記録
    struct RankingEntry {
        int32_t total = 0;      // 総合スコア。並び替えの基準になる
        int32_t score = 0;      // そのときのスコア
        float   seconds = 0.0f; // そのときの生存時間（秒）
    };

    /// ランキングに残す件数
    static constexpr size_t RANKING_COUNT = 5;

private:

    /// エディタが持つ値。演出はこの値を土台にして毎フレーム計算する
    struct ElementBase {
        Vector2 position {};
        Vector2 size     {};
        Vector4 color    {};
        float   fontSize = 0.0f;
    };

    // ─── 表示状態 ──────────────────────────────────────────────
    bool    active_ = false;        // シートを出しているか
    float   elapsed_ = 0.0f;        // Show() からの経過秒数。全ての演出はこれ基準
    float   resultSeconds_ = 0.0f;  // 表示する生存時間（秒）
    int32_t resultScore_ = 0;       // 表示するスコア
    int32_t resultTotal_ = 0;       // 表示する総合スコア
    bool    returnAccepting_ = false; // タイトルへ戻る操作を受け付けているか
    bool    animating_ = false;     // 演出中で、要素へ書き戻している最中か

    // ─── 演出の設定（Assets/Data/Result/Result.json）────────────
    float dimStrength_ = 0.72f;         // 暗幕の濃さ。1.0 で背景が完全に隠れる
    float dimFadeDuration_ = 0.45f;     // 暗幕が濃くなりきるまでの秒数
    float panelDelay_ = 0.15f;          // 暗幕が出てからシートが出るまでの秒数
    float panelDuration_ = 0.45f;       // シートが着地するまでの秒数
    float panelStartScale_ = 1.12f;     // 着地前の拡大率。1.0 で演出なし
    float rowDelay_ = 0.5f;             // シートが出てから1行目が出るまでの秒数
    float rowInterval_ = 0.22f;         // 次の行が出るまでの間隔
    float rowFadeDuration_ = 0.3f;      // 1行が出きるまでの秒数
    float rowRiseDistance_ = 16.0f;     // 出現時に何ピクセル下から浮き上がるか
    float countUpDuration_ = 0.7f;      // 数字が最終値まで回りきる秒数
    int32_t scoreDigits_ = 6;           // スコアを0埋めする桁数。0以下なら0埋めしない
    float rankingInterval_ = 0.09f;     // ランキングが1行ずつ出てくる間隔
    float returnDelay_ = 1.2f;          // 「タイトルへ戻る」を出すまでの秒数
    float returnFadeDuration_ = 0.3f;   // それが出きるまでの秒数

    // ─── 総合スコアの算出（Assets/Data/Result/Result.json）──────
    /// 総合スコア = スコア × scoreWeight_ + 切り捨てた生存秒数 × timeBonusPerSecond_
    /// @note 敵1体が 100 点なので、既定値の 100/秒 は「1秒守り切る = 敵1体分」という重み付け
    float   scoreWeight_ = 1.0f;
    int32_t timeBonusPerSecond_ = 100;
    int32_t totalDigits_ = 7;           // 総合スコアを0埋めする桁数

    // ─── ランキング（Assets/Data/Ranking/Ranking.json）──────────
    /// 総合スコアの高い順。最大 RANKING_COUNT 件
    std::vector<RankingEntry> ranking_;
    /// 今回の記録が入った順位（0始まり）。ランク外なら -1
    int32_t newRankIndex_ = -1;

    // ─── UI（Assets/Data/UI/Result.json）───────────────────────
    Ui::Canvas canvas_ {};
    /// 要素名 → エディタが持つ値
    std::unordered_map<std::string, ElementBase> bases_;
    /// 拡大縮小の中心。Panel 要素の位置をそのまま使う
    Vector2 panelCenter_ {640.0f, 360.0f};
    /// エディタの色ではなくこちらの色で出したい要素（今回の記録を目立たせるのに使う）
    std::unordered_map<std::string, Vector4> colorOverrides_;
    /// 「タイトルへ戻る」が押されたときに呼ぶ処理
    std::function<void()> onReturn_;

public:
    /// JSON を読み込み、Canvas を閉じた状態で用意する
    void Initialize();

    /// 「タイトルへ戻る」が選ばれたときの処理を登録する
    /// @note ボタンのクリックからも呼ばれるので、Initialize() の後に必ず設定すること
    void SetOnReturn(std::function<void()> _onReturn);

    /// 出現演出とカウントアップを進める
    /// @param _deltaTime 前フレームからの経過秒数
    /// @note ゲーム側を止めていてもリザルトは動かしたいので、
    ///       ここには止めていない実時間を渡すこと
    void Update(float _deltaTime);

    /// リザルトを表示する
    /// @param _survivedSeconds 守り切った時間（秒）
    /// @param _score           最終スコア
    /// @note 表示中に呼んでも無視されるので、毎フレーム呼んでも演出が巻き戻らない
    void Show(float _survivedSeconds, int32_t _score);

    /// リザルトを閉じる（リトライ時などに使用）
    void Hide();

    // ─── Getter ────────────────────────────────────────────────

    /// @brief リザルト表示中かどうか
    /// @note ゲーム側の更新を止める判定に使う
    bool IsActive() const { return active_; }

    /// @brief タイトルへ戻る操作を受け付けているか
    /// @note 倒された瞬間のクリックでそのまま飛ばないよう、少し待ってから true になる
    bool IsReturnAccepting() const { return returnAccepting_; }

    /// @brief 今回の総合スコア
    int32_t GetTotalScore() const { return resultTotal_; }

    /// @brief 総合スコアの高い順に並んだランキング
    const std::vector<RankingEntry>& GetRanking() const { return ranking_; }

    /// @brief 今回の記録が入った順位（0始まり）。ランク外なら -1
    int32_t GetNewRankIndex() const { return newRankIndex_; }

private:
    /// @brief Assets/Data/Result/Result.json から演出のパラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief エディタが持つ値を控える。演出はこの値を土台に計算する
    /// @note 起動時に1回だけ呼ぶ。演出後の値を控え直すと表示のたびに色がズレていく
    void CaptureBases();

    /// @brief 控えた値を要素へ書き戻して、1フレーム分の演出を作る
    void Apply();

    /// @brief シートと一緒に動く要素（枠・地・帯・線・見出し）を配置する
    /// @param _scale  シートの拡大率
    /// @param _appear 出現の進捗 [0,1]
    void ApplyPanel(float _scale, float _appear);

    /// @brief 成績1行を配置し、数値をカウントアップさせる
    /// @param _row   何行目か（出現の時間差に使う）
    /// @param _scale シートの拡大率
    void ApplyRow(Row _row, float _scale);

    /// @brief ランキングを配置する。見出しのあとに1行ずつ出す
    /// @param _scale シートの拡大率
    void ApplyRanking(float _scale);

    /// @brief 「タイトルへ戻る」を配置する。受付が始まるまでは隠しておく
    /// @param _scale シートの拡大率
    void ApplyReturn(float _scale);

    /// @brief スコアと生存時間から総合スコアを出す
    int32_t CalcTotalScore(float _seconds, int32_t _score) const;

    /// @brief Assets/Data/Ranking/Ranking.json からランキングを読み込む
    void LoadRanking();

    /// @brief ランキングを Assets/Data/Ranking/Ranking.json へ書き出す
    void SaveRanking() const;

    /// @brief 今回の記録をランキングへ入れ、入った順位を newRankIndex_ に控える
    void InsertRanking(const RankingEntry& _entry);

    /// @brief ランキングの文字を要素へ流し込む
    void RefreshRankingTexts();

    /// @brief ランキングの見出しが出始めるまでの秒数
    float GetRankingDelay() const;

    /// @brief 名前で要素を引き、控えた値と一緒に返す
    /// @note エディタの Reload で要素が作り直されるため、ポインタは持たず毎回引き直す
    /// @return 要素が無い、または値を控えていない場合は false
    bool FindElement(const std::string& _name, Ui::Element*& _outElement,
                     const ElementBase*& _outBase) const;

    /// @brief 使う色を返す。上書きが登録されていればエディタの色より優先する
    const Vector4& BaseColorOf(const std::string& _name, const Vector4& _base) const;

    /// @brief 中心からの距離を拡大率でスケールした位置を返す
    Vector2 ScaledPosition(const Vector2& _base, float _scale) const;

    /// @brief 経過時間を "分:秒" の文字列にする
    std::string MakeTimeText(float _seconds) const;

    /// @brief 数値を指定した桁数まで0埋めした文字列にする
    /// @note カウントアップ中に文字数が変わって位置が動くのを防ぐ
    std::string MakePaddedText(int32_t _value, int32_t _digits) const;

    /// @brief _delay 秒後から _duration 秒かけて 0→1 になる進捗を返す
    float CalcProgress(float _delay, float _duration) const;

    /// @brief 指定した行が出現し始めるまでの秒数
    float GetRowDelay(Row _row) const;

    /// @brief 全ての演出が終わるまでの秒数
    float GetTotalDuration() const;
};

#endif // RESULT_OVERLAY_HPP_
