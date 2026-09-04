#ifndef SURVIVAL_TIME_MANAGER_HPP_
#define SURVIVAL_TIME_MANAGER_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

/** 「何秒タワーを守れたか」を数える生存時間の管理と UI 表示を受け持つクラス
 *
 *  制限時間ではなくカウントアップなので、時間切れという概念は無い。
 *  ゲームが終わるのはタワーの HP が尽きたときだけで、
 *  この時間は「どれだけ耐えたか」という成績になる。
 *
 *  ## なぜリング型なのか
 *  タワーHPが横長のバーなので、時間も横バーにすると形が同じで見分けがつかない。
 *  時計を連想させる円形にすることで、一目でどちらの情報か分かるようにしている。
 *
 *  ## 目盛りの意味
 *  リング1周 = secondsPerLap_ 秒（既定60秒）で、tickCount_ 個の目盛りに分割される。
 *  既定では 60秒 ÷ 20目盛り = 3秒/目盛り。3秒ごとに1つずつ点灯していく。
 *
 *  1周し終える（＝1分耐えた）とリング全体が光り、そのままの色で満タンを保つ。
 *  次の1分は新しい色が上から重なっていくので、リングは空に戻らず
 *  「何色まで到達したか」がそのまま耐えた分数の記録になる。
 *  中央の数値は経過時間の合計を "分:秒" で出しているので、
 *  リングは「今の1分のどこか＋何周したか」、数値は「通算どれだけ耐えたか」を表す。
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/SurvivalTime/SurvivalTime.json から
 *  上書きされる。配置・色・演出は再ビルドせずに JSON の値だけで調整できる。
 */
class SurvivalTimeManager final {
    /// 目盛りスプライトの確保数（tickCount_ はこれを超えられない）
    static constexpr size_t TICK_MAX = 64;

    // ─── 生存時間 ──────────────────────────────────────────────
    float elapsedSeconds_ = 0.0f;       // 経過時間（秒）。守り切った長さそのもの
    bool counting_ = true;              // false の間はカウントが止まる（ポーズ用）

    // ─── リングの状態 ──────────────────────────────────────────
    int32_t litCount_ = 0;              // 現在点灯している目盛りの数
    int32_t lapCount_ = 0;              // 何周したか（＝何分耐えたか）
    /// 目盛りごとの「点灯した直後」の残り時間。灯った瞬間を白く光らせるのに使う
    std::array<float, TICK_MAX> tickGainTimers_{};

    // ─── 演出用タイマー ────────────────────────────────────────
    float lapFlashTimer_ = 0.0f;        // 1周し終えた瞬間のフラッシュの残り時間
    float punchTimer_ = 0.0f;           // パンチスケールの残り時間

    // ─── リングの見た目設定 ────────────────────────────────────
    Vector2 ringCenter_{104.0f, 96.0f};         // リングの中心のピクセル座標
    float ringRadius_ = 50.0f;                  // 中心から目盛りの中心までの距離
    Vector2 tickSize_{7.0f, 16.0f};             // 目盛り1個のサイズ（x=太さ, y=長さ）
    int32_t tickCount_ = 20;                    // 目盛りの数。1目盛り = secondsPerLap_ / これ 秒
    float secondsPerLap_ = 60.0f;               // リングが1周する秒数
    Vector4 emptyColor_{0.13f, 0.13f, 0.17f, 0.85f};    // まだ1周もしていない部分の色
    Vector4 gainColor_{1.0f, 1.0f, 1.0f, 1.0f};         // 灯った直後の目盛りの色
    /// 周回ごとの目盛りの色。耐えた分数が進むほど先の色になり、使い切ったら先頭へ戻る
    std::vector<Vector4> lapColors_{
        {0.25f, 0.95f, 0.55f, 1.0f},    // 1分目: 緑
        {1.0f,  0.85f, 0.30f, 1.0f},    // 2分目: 黄
        {1.0f,  0.55f, 0.25f, 1.0f},    // 3分目: 橙
        {1.0f,  0.40f, 0.70f, 1.0f},    // 4分目: 桃
        {0.70f, 0.50f, 1.0f,  1.0f},    // 5分目: 紫
    };
    float gainFlashDuration_ = 0.5f;    // 灯った目盛りが白く光っている秒数

    // ─── 「1周した瞬間」の演出設定 ─────────────────────────────
    float lapFlashDuration_ = 0.6f;     // 1周したときのフラッシュの長さ（秒）
    float punchDuration_ = 0.35f;       // パンチスケールの長さ（秒）
    float punchScale_ = 1.5f;           // 目盛りが伸びる倍率。1.0 で演出なし
    float valuePunchScale_ = 1.3f;      // 1周した瞬間に数値を何倍まで大きくするか

    // ─── 数値表示の設定 ────────────────────────────────────────
    // リングの中心へ置くので中央揃えにする（Text は左揃えしかできないので幅を見積もる）
    Text valueText_{};
    float valueFontSize_ = 30.0f;               // 通常時のフォントサイズ
    int32_t minuteDigits_ = 2;                  // 分に確保しておく桁数（1桁のうちは空けておく）
    float valueOffsetY_ = -20.0f;               // リング中心から見たテキスト上端のずれ
    Vector4 valueColor_{0.9f, 1.0f, 0.95f, 1.0f};       // 文字色
    float charWidthRatio_ = 0.53f;              // 中央揃えに使う「1文字幅 ÷ フォントサイズ」の目安

    // ─── ラベル（"TIME"）の設定 ────────────────────────────────
    Text labelText_{};
    std::string label_{"TIME"};                 // リングの下に出す見出し
    float labelFontSize_ = 20.0f;               // フォントサイズ
    float labelOffsetY_ = 66.0f;                // リング中心から見たテキスト上端のずれ
    Vector4 labelColor_{0.75f, 0.8f, 0.88f, 1.0f};      // 文字色

    // ─── 描画に使うスプライト ──────────────────────────────────
    /// リングの目盛り。先頭 tickCount_ 個だけを初期化して使う
    std::array<Sprite, TICK_MAX> tickSprites_{};

    bool visible_ = true;           // UI 全体の表示 / 非表示

public:
    /// JSON からパラメータを読み込み、リングとテキストを初期化する
    void Initialize();

    /// 経過時間を進め、UI と演出を更新する
    /// _deltaTime 前フレームからの経過秒数
    void Update(float _deltaTime);

    /// リングと数値を描画キューへ積む
    /// スプライトを使うので、3D の描画がすべて終わったあとに呼ぶ
    void Draw();

    /// 経過時間を 0 に戻す（リトライ時などに使用）
    void Reset();

    // ─── Getter / Setter ───────────────────────────────────────

    /// @brief 守り切った時間（秒）
    /// @note リザルト画面などで成績として使う
    float GetElapsedSeconds() const { return elapsedSeconds_; }

    /// @brief 何周したか（＝何分耐えたか）
    int32_t GetLapCount() const { return lapCount_; }

    /// @brief カウントの一時停止 / 再開
    void SetCounting(bool _counting) { counting_ = _counting; }

    /// @brief UI の表示 / 非表示を切り替える
    void SetVisible(bool _visible);

private:
    /// @brief Assets/Data/SurvivalTime/SurvivalTime.json から各パラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 点灯数の更新とタイマー類を進める
    void UpdateRing(float _deltaTime);

    /// @brief 現在の状態を目盛りスプライトの位置・サイズ・色へ反映する
    void ApplyTickSprites();

    /// @brief 経過時間のテキストを更新する
    void RefreshValueText();

    /// @brief 今の周回で点灯しているべき目盛り数を求める
    int32_t CalcLitCount() const;

    /// @brief 指定した周回の色を返す（色を使い切ったら先頭から繰り返す）
    /// @param _lap 0 が1分目
    Vector4 GetLapColor(int32_t _lap) const;

    /// @brief パンチスケールの現在倍率を返す（演出中でなければ 1.0）
    float GetPunchScale() const;

    /// @brief 1周フラッシュの現在の濃度を返す（演出中でなければ 0.0）
    float GetLapFlashAlpha() const;

    /// @brief 文字列の描画幅を概算する（Text に中央揃え機能が無いため自前で見積もる）
    float EstimateTextWidth(const std::string& _text, float _fontSize) const;
};

#endif // SURVIVAL_TIME_MANAGER_HPP_
