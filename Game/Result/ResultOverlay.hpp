#ifndef RESULT_OVERLAY_HPP_
#define RESULT_OVERLAY_HPP_

#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

/** タワーが落ちたときに成績を見せるリザルトシート
 *
 *  ## なぜシーンを切り替えないのか
 *  リザルト専用シーンへ飛ぶと、プレイ中の画面が一度消えてしまい
 *  「どこで負けたのか」がプレイヤーの記憶からも画面からも失われる。
 *  そのため PlayScene のまま画面全体を暗幕（dim）で覆い、その上に
 *  シートだけを重ねている。うっすら透けるゲーム画面が最後の状況を残し、
 *  そのままリトライへ繋げられる。
 *
 *  ## 見せる順番
 *  一度に全部出すと数字が読み飛ばされるので、時間差で出している。
 *    1. 暗幕がフェードイン        … 操作が終わったことを伝える
 *    2. シートが少し縮みながら着地 … 視線をシートの中央へ集める
 *    3. TIME → SCORE の順に出現   … 1行ずつ読ませる
 *    4. 数字がカウントアップ       … 成績が積み上がる手応えを出す
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/Result/Result.json から上書きされる。
 *  配置・色・演出の速さは再ビルドせずに JSON の値だけで調整できる。
 */
class ResultOverlay final {
    /// 成績1行（ラベルと数値の組）の識別子。表示順もこの並びになる
    enum class Row : int32_t {
        Time = 0,
        Score = 1,
    };

    // ─── 表示状態 ──────────────────────────────────────────────
    bool active_ = false;           // シートを出しているか
    float elapsed_ = 0.0f;          // Show() からの経過秒数。全ての演出はこれ基準
    float resultSeconds_ = 0.0f;    // 表示する生存時間（秒）
    int32_t resultScore_ = 0;       // 表示するスコア

    // ─── 暗幕の設定 ────────────────────────────────────────────
    Vector4 dimColor_{0.0f, 0.0f, 0.02f, 1.0f}; // 暗幕の色
    float dimStrength_ = 0.72f;                 // 最終的な濃さ。1.0 で背景が完全に隠れる
    float dimFadeDuration_ = 0.45f;             // 暗幕が濃くなりきるまでの秒数

    // ─── シート本体の設定 ──────────────────────────────────────
    Vector2 panelCenter_{640.0f, 360.0f};               // シートの中心のピクセル座標
    Vector2 panelSize_{520.0f, 320.0f};                 // シートの大きさ
    /// シートの地色。縁取りの板の上に重ねるので、半透明にすると縁の色が透けて濁る
    Vector4 panelColor_{0.06f, 0.07f, 0.10f, 1.0f};
    float panelBorder_ = 3.0f;                          // 外周の縁取りの太さ
    Vector4 panelBorderColor_{0.25f, 0.95f, 0.55f, 0.9f};   // 縁取りの色
    float accentHeight_ = 6.0f;                         // 上端に敷くアクセント帯の高さ
    Vector4 accentColor_{0.25f, 0.95f, 0.55f, 1.0f};    // アクセント帯の色
    float panelDelay_ = 0.15f;                          // 暗幕が出てからシートが出るまでの秒数
    float panelDuration_ = 0.45f;                       // シートが着地するまでの秒数
    float panelStartScale_ = 1.12f;                     // 着地前の拡大率。1.0 で演出なし

    // ─── 見出しの設定 ──────────────────────────────────────────
    Text titleText_{};
    std::string title_{"RESULT"};                       // 見出しの文字列
    float titleFontSize_ = 52.0f;                       // フォントサイズ
    float titleOffsetY_ = -132.0f;                      // シート中心から見たテキスト上端のずれ
    Vector4 titleColor_{1.0f, 1.0f, 1.0f, 1.0f};        // 文字色

    // ─── 見出しの下の区切り線 ──────────────────────────────────
    float dividerOffsetY_ = -50.0f;                     // シート中心から見た線の中心のずれ
    Vector2 dividerSize_{400.0f, 2.0f};                 // 線の大きさ
    Vector4 dividerColor_{1.0f, 1.0f, 1.0f, 0.25f};     // 線の色

    // ─── 成績1行の設定 ─────────────────────────────────────────
    /// ラベルと数値を別々の列に左揃えで並べ、表のように読ませる。
    /// Text に幅を測る手段が無く、フォントも字ごとに幅が違うため、
    /// 右揃えにすると "0:13" と "000300" で右端が揃わない。左揃えなら常に正確に揃う
    float rowFirstOffsetY_ = 0.0f;                      // 1行目のラベル上端のずれ
    float rowSpacing_ = 78.0f;                          // 行と行の間隔
    float labelOffsetX_ = -200.0f;                      // 中心から見たラベル左端の位置
    float valueOffsetX_ = 20.0f;                        // 中心から見た数値左端の位置
    float labelFontSize_ = 26.0f;                       // ラベルのフォントサイズ
    Vector4 labelColor_{0.72f, 0.78f, 0.88f, 1.0f};     // ラベルの文字色
    float valueFontSize_ = 46.0f;                       // 数値のフォントサイズ
    Vector4 valueColor_{1.0f, 1.0f, 1.0f, 1.0f};        // 数値の文字色
    std::string timeLabel_{"TIME"};                     // 1行目のラベル
    std::string scoreLabel_{"SCORE"};                   // 2行目のラベル
    int32_t scoreDigits_ = 6;                           // スコアを0埋めする桁数。0以下なら0埋めしない
    float charWidthRatio_ = 0.53f;                      // 見出しの中央揃えに使う「1文字幅 ÷ フォントサイズ」の目安

    // ─── 成績が出てくる演出の設定 ──────────────────────────────
    float rowDelay_ = 0.5f;             // シートが出てから1行目が出るまでの秒数
    float rowInterval_ = 0.22f;         // 次の行が出るまでの間隔
    float rowFadeDuration_ = 0.3f;      // 1行が出きるまでの秒数
    float rowRiseDistance_ = 16.0f;     // 出現時に何ピクセル下から浮き上がるか
    float countUpDuration_ = 0.7f;      // 数字が最終値まで回りきる秒数

    // ─── 描画に使うスプライト ──────────────────────────────────
    Sprite dimSprite_{};            // 画面全体を覆う暗幕
    Sprite panelBorderSprite_{};    // シートの縁取り（本体より一回り大きい板）
    Sprite panelSprite_{};          // シートの地
    Sprite accentSprite_{};         // シート上端のアクセント帯
    Sprite dividerSprite_{};        // 見出しの下の区切り線

    // ─── 成績表示用のテキスト ──────────────────────────────────
    Text timeLabelText_{};
    Text timeValueText_{};
    Text scoreLabelText_{};
    Text scoreValueText_{};

public:
    /// JSON からパラメータを読み込み、スプライトとテキストを初期化する
    void Initialize();

    /// 出現演出とカウントアップを進める
    /// _deltaTime 前フレームからの経過秒数
    /// @note ゲーム側を止めていてもリザルトは動かしたいので、
    ///       ここには止めていない実時間を渡すこと
    void Update(float _deltaTime);

    /// 暗幕とシートを描画キューへ積む
    /// @note ゲーム中の UI をすべて描いたあとに呼ぶと、UI ごと暗幕の下に沈む
    void Draw();

    /// リザルトを表示する
    /// _survivedSeconds 守り切った時間（秒）
    /// _score           最終スコア
    /// @note 表示中に呼んでも無視されるので、毎フレーム呼んでも演出が巻き戻らない
    void Show(float _survivedSeconds, int32_t _score);

    /// リザルトを閉じる（リトライ時などに使用）
    void Hide();

    // ─── Getter ────────────────────────────────────────────────

    /// @brief リザルト表示中かどうか
    /// @note ゲーム側の更新を止める判定に使う
    bool IsActive() const { return active_; }

private:
    /// @brief Assets/Data/Result/Result.json から各パラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 暗幕・シート・区切り線の位置とサイズと色を更新する
    void ApplySprites();

    /// @brief 見出しと成績のテキストを更新する
    void RefreshTexts();

    /// @brief 成績1行分のテキストを配置する
    /// @param _label     ラベル側のテキスト
    /// @param _value     数値側のテキスト
    /// @param _valueText 表示する数値の文字列
    /// @param _row       何行目か（出現の時間差と縦位置に使う）
    /// @param _scale     シートの拡大率（着地演出に合わせて文字も一緒に伸縮させる）
    void ApplyRow(Text& _label, Text& _value, const std::string& _valueText,
                  Row _row, float _scale);

    /// @brief 経過時間を "分:秒" の文字列にする
    std::string MakeTimeText(float _seconds) const;

    /// @brief スコアを0埋めした文字列にする
    std::string MakeScoreText(int32_t _score) const;

    /// @brief _delay 秒後から _duration 秒かけて 0→1 になる進捗を返す
    float CalcProgress(float _delay, float _duration) const;

    /// @brief 指定した行が出現し始めるまでの秒数
    float GetRowDelay(Row _row) const;

    /// @brief 文字列の描画幅を概算する（Text に中央揃え機能が無いため自前で見積もる）
    /// @note 字ごとの幅までは分からないので、中央揃えの見出しにだけ使う
    float EstimateTextWidth(const std::string& _text, float _fontSize) const;
};

#endif // RESULT_OVERLAY_HPP_
