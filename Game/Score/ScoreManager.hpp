#ifndef SCORE_MANAGER_HPP_
#define SCORE_MANAGER_HPP_

#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Text/Text.hpp"

/** スコアの保持・加算・画面表示をまとめて受け持つクラス
 *
 *  敵1体あたりの点数は Enemy / EnemyManager 側が持つ
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/Score/Score.json から上書きされる。
 *  表示位置・フォントサイズ・倍率などは再ビルドせずに JSON の値だけで調整できる。
 */
class ScoreManager final {
    // ─── スコアの実データ ──────────────────────────────────────
    int32_t score_ = 0;             // 現在のスコア（実値）
    int32_t initialScore_ = 0;      // Reset() で戻す初期スコア
    int32_t scoreMultiplier_ = 1;   // 加算時に掛ける倍率（2にすれば獲得点がすべて2倍）

    // ─── 表示用のカウントアップ演出 ────────────────────────────
    float displayScore_ = 0.0f;     // 実際に画面へ出している値。score_ へ徐々に近づく
    float countUpSpeed_ = 600.0f;   // 1秒あたりのカウントアップ量。0以下なら即座に反映

    // ─── 表示設定 ──────────────────────────────────────────────
    Text textObject_{};                         // スコア表示用のテキスト
    std::string label_{"SCORE "};               // 数字の前に付ける文字列
    Vector2 textPosition_{32.0f, 24.0f};        // 画面左上を原点としたピクセル座標
    float fontSize_ = 32.0f;                    // フォントサイズ（ピクセル）
    Vector4 textColor_{1.0f, 1.0f, 1.0f, 1.0f}; // 文字色（RGBA 0.0〜1.0）
    int32_t displayDigits_ = 6;                 // 0埋めする桁数。0以下なら0埋めしない

public:
    /// JSON からパラメータを読み込み、表示用テキストを初期化する
    void Initialize();

    /// カウントアップ演出を進める
    /// _deltaTime 前フレームからの経過秒数
    void Update(float _deltaTime);

    /// このフレームの描画キューにスコア表示を積む
    void Draw();

    /// スコアを加算する
    /// _score 加算する素点（内部で scoreMultiplier_ が掛けられる）
    /// 敵を倒したとき以外（ボーナスなど）からもこの関数を呼べばよい
    void AddScore(int32_t _score);

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
    void SetVisible(bool _visible) { textObject_.SetVisible(_visible); }

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
};

#endif // SCORE_MANAGER_HPP_
