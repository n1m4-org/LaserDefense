#ifndef SCORE_MANAGER_HPP_
#define SCORE_MANAGER_HPP_

#include <array>
#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Text/Text.hpp"

/** スコアの保持・加算・画面表示をまとめて受け持つクラス
 *
 *  敵1体あたりの点数は Enemy / EnemyManager 側が持つ
 *
 *  ## 加算時の演出
 *  TimeLimitManager / ComboManager と同じ方針で、視線を向けていなくても
 *  「入った」ことが分かるようにしている。
 *    1. ポップアップ  … "+300" を数字の下に出し、スコアへ吸い込まれるように浮かせる
 *    2. フラッシュ    … スコアの文字色を一瞬明るいアクセント色へ振る（輝度の急変）
 *    3. パンチスケール … スコアの文字が一瞬大きくなって戻る（動き）
 *    4. カウントアップ … 数字が一定速度で増えるので、大量得点ほど長く回り続ける
 *  さらにコンボ倍率が高いほど演出を強く（文字を大きく・色を暖色へ）して、
 *  大きく稼いだことが一目で分かるようにしている。
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/Score/Score.json から上書きされる。
 *  表示位置・フォントサイズ・倍率などは再ビルドせずに JSON の値だけで調整できる。
 */
class ScoreManager final {
    /// 同時に表示できる「+○○」ポップアップの最大数
    static constexpr size_t POPUP_COUNT = 5;

    /// 「+○○」ポップアップ1個分の状態
    struct GainPopup {
        Text text{};              // 表示用テキスト
        float elapsed = 0.0f;     // 表示開始からの経過秒数
        float baseY = 0.0f;       // 浮き上がりの起点になる Y 座標
        float strength = 0.0f;    // 演出の強さ（0=通常, 1=最大倍率）
        bool active = false;      // 表示中かどうか
    };

    // ─── スコアの実データ ──────────────────────────────────────
    int32_t score_ = 0;             // 現在のスコア（実値）
    int32_t initialScore_ = 0;      // Reset() で戻す初期スコア
    int32_t scoreMultiplier_ = 1;   // 加算時に掛ける倍率（2にすれば獲得点がすべて2倍）

    // ─── 表示用のカウントアップ演出 ────────────────────────────
    float displayScore_ = 0.0f;     // 実際に画面へ出している値。score_ へ徐々に近づく
    float countUpSpeed_ = 600.0f;   // 1秒あたりのカウントアップ量。0以下なら即座に反映

    // ─── 加算時の演出タイマー ──────────────────────────────────
    float punchTimer_ = 0.0f;       // パンチスケールの残り時間
    float flashTimer_ = 0.0f;       // 文字色フラッシュの残り時間
    float lastStrength_ = 0.0f;     // 直近の加算の強さ（0〜1）。倍率が高いほど大きい

    // ─── 加算時の演出設定 ──────────────────────────────────────
    float punchDuration_ = 0.3f;    // パンチスケールの長さ（秒）
    float punchScale_ = 1.25f;      // 通常の加算で膨らむ倍率
    float punchScaleHigh_ = 1.7f;   // 最大倍率での加算で膨らむ倍率
    float flashDuration_ = 0.25f;   // 文字色フラッシュの長さ（秒）
    int32_t strengthMax_ = 8;       // 演出の強さを正規化する基準（コンボ倍率の上限に合わせる）

    // ─── 表示設定 ──────────────────────────────────────────────
    Text textObject_{};                             // スコア表示用のテキスト
    std::string label_{"SCORE "};                   // 数字の前に付ける文字列
    Vector2 textPosition_{995.0f, 24.0f};           // 画面左上を原点としたピクセル座標
    float fontSize_ = 40.0f;                        // フォントサイズ（ピクセル）
    Vector4 textColor_{1.0f, 1.0f, 1.0f, 1.0f};     // 通常時の文字色（RGBA 0.0〜1.0）
    Vector4 gainColor_{0.75f, 1.0f, 0.85f, 1.0f};   // 加算直後に一瞬振れる色
    Vector4 highGainColor_{1.0f, 0.6f, 0.2f, 1.0f}; // 高倍率で加算したときに振れる色
    int32_t displayDigits_ = 6;                     // 0埋めする桁数。0以下なら0埋めしない
    float textRightEdge_ = 0.0f;                    // 右揃えの基準になる右端 X 座標（位置とフォントから自動算出）

    // ─── 「+○○」ポップアップの設定 ────────────────────────────
    std::array<GainPopup, POPUP_COUNT> popups_{};   // 使い回すポップアップ
    float popupTopY_ = 78.0f;                       // ポップアップの出現 Y 座標
    float popupFontSize_ = 30.0f;                   // 通常時のフォントサイズ
    float popupFontSizeHigh_ = 46.0f;               // 最大倍率でのフォントサイズ
    Vector4 popupColor_{0.75f, 1.0f, 0.85f, 1.0f};  // 通常時の色
    Vector4 popupHighColor_{1.0f, 0.6f, 0.2f, 1.0f};// 最大倍率での色
    float popupRiseDistance_ = 46.0f;               // 消えるまでに何ピクセル上へ浮くか
    float popupDuration_ = 0.85f;                   // 表示してから消えるまでの秒数
    float popupStackOffset_ = 30.0f;                // 同時表示が重なったときに縦へずらす量
    float charWidthRatio_ = 0.53f;                  // 右揃えに使う「1文字幅 ÷ フォントサイズ」の目安

public:
    /// JSON からパラメータを読み込み、表示用テキストを初期化する
    void Initialize();

    /// カウントアップ演出を進める
    /// _deltaTime 前フレームからの経過秒数
    void Update(float _deltaTime);

    /// このフレームの描画キューにスコア表示を積む
    void Draw();

    /// スコアを加算する
    /// _score    加算する素点（内部で scoreMultiplier_ が掛けられる）
    /// _emphasis 演出の強さに使うコンボ倍率。1なら通常、大きいほど派手になる
    /// 敵を倒したとき以外（ボーナスなど）からもこの関数を呼べばよい
    void AddScore(int32_t _score, int32_t _emphasis = 1);

    /// スコアを初期値へ戻す（リトライ時などに使用）
    void Reset();

    // ─── Getter / Setter ───────────────────────────────────────

    int32_t GetScore() const { return score_; }

    /// スコアを任意の値へ直接設定する（デバッグ用途など）
    void SetScore(int32_t _score);

    int32_t GetScoreMultiplier() const { return scoreMultiplier_; }

    /// 獲得スコアの倍率を変更する（アイテム効果などで使用）
    void SetScoreMultiplier(int32_t _multiplier) { scoreMultiplier_ = _multiplier; }

    /// スコア表示の表示 / 非表示を切り替える
    void SetVisible(bool _visible);

private:
    /// Assets/Data/Score/Score.json から各パラメータを読み込む
    /// ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// 位置・サイズ・色といった表示設定を Text へ反映する
    void ApplyTextSettings();

    // displayScore_ の内容を表示文字列へ変換して Text に反映する
    void RefreshText();

    //ラベルと0埋めを適用した表示文字列を作る
    std::string MakeDisplayString() const;

    /// ポップアップの浮き上がりとフェードを進める
    void UpdatePopups(float _deltaTime);

    /// 空いているポップアップを1つ使って「+○○」を表示する
    /// _gained   加算された点数（倍率適用後）
    /// _strength 演出の強さ（0〜1）
    void SpawnGainPopup(int32_t _gained, float _strength);

    /// 文字列の描画幅を概算する（Text に右揃え機能が無いため自前で見積もる）
    float EstimateTextWidth(const std::string& _text, float _fontSize) const;

    /// パンチスケールの現在倍率を返す（演出中でなければ 1.0）
    float GetPunchScale() const;

    /// 加算直後の色。演出が終わっていれば通常色を返す
    Vector4 GetTextColor() const;
};

#endif // SCORE_MANAGER_HPP_
