#ifndef TIME_LIMIT_MANAGER_HPP_
#define TIME_LIMIT_MANAGER_HPP_

#include <array>
#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

/** 制限時間の管理と UI 表示（ゲージ＋数値）を受け持つクラス
 *

 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/TimeLimit/TimeLimit.json から上書きされる。
 *  演出の強さ・速さ・色・配置は再ビルドせずに JSON の値だけで調整できる。
 */
class TimeLimitManager final {
    /// 同時に表示できる「+○○s」ポップアップの最大数
    static constexpr size_t POPUP_COUNT = 4;

    /** 「+○○s」ポップアップ1個分の状態 */
    struct GainPopup {
        Text text{};              // 表示用テキスト
        float elapsed = 0.0f;     // 表示開始からの経過秒数
        float baseY = 0.0f;       // 浮き上がりの起点になる Y 座標
        bool active = false;      // 表示中かどうか
    };

    // ─── 制限時間のルール ──────────────────────────────────────
    float remainingSeconds_ = 30.0f;    // 現在の残り時間（秒）
    float startSeconds_ = 30.0f;        // 開始時 / Reset() 時の残り時間
    float maxSeconds_ = 99.0f;          // 残り時間の上限。ゲージ満タンの基準にもなる
    bool countingDown_ = true;          // false の間はカウントダウンが止まる（ポーズ用）

    // ─── ゲージの表示状態 ──────────────────────────────────────
    float displayRatio_ = 1.0f;         // 実際に描いているゲージの割合（0〜1）
    float fillDuration_ = 0.45f;        // 増加分を本体バーが埋めきるまでの秒数
    float fillTimer_ = 0.0f;            // 埋めるアニメーションの残り時間
    float fillStartRatio_ = 1.0f;       // 埋めるアニメーションを始めたときの割合

    // ─── 演出用タイマー ────────────────────────────────────────
    float flashTimer_ = 0.0f;           // フラッシュの残り時間
    float punchTimer_ = 0.0f;           // パンチスケールの残り時間
    float blinkTime_ = 0.0f;            // 危険時の明滅に使う経過時間

    // ─── ゲージの見た目設定 ────────────────────────────────────
    Vector2 gaugePosition_{32.0f, 150.0f};      // ゲージ左端・中心のピクセル座標
    Vector2 gaugeSize_{420.0f, 26.0f};          // ゲージ本体のサイズ（満タン時）
    float frameThickness_ = 4.0f;               // ゲージを囲む枠の太さ
    Vector4 frameColor_{0.05f, 0.05f, 0.08f, 0.85f};    // 枠 兼 空き部分の色
    Vector4 safeColor_{0.25f, 0.95f, 0.55f, 1.0f};      // 残り時間が十分あるときの色
    Vector4 warningColor_{1.0f, 0.85f, 0.25f, 1.0f};    // 残り時間が減ってきたときの色
    Vector4 dangerColor_{1.0f, 0.28f, 0.28f, 1.0f};     // 残り時間が危険なときの色
    Vector4 ghostColor_{1.0f, 1.0f, 1.0f, 0.9f};        // 増えた分を先行表示するゴーストバーの色
    float warningRatio_ = 0.4f;         // この割合を下回ると warningColor_ になる
    float dangerRatio_ = 0.2f;          // この割合を下回ると dangerColor_ になり明滅する

    // ─── 「増えた瞬間」の演出設定 ──────────────────────────────
    float flashDuration_ = 0.35f;       // フラッシュの長さ（秒）
    float flashStrength_ = 0.85f;       // フラッシュの最大濃度（0〜1）
    float punchDuration_ = 0.32f;       // パンチスケールの長さ（秒）
    float punchScale_ = 1.5f;           // 膨らむ倍率。1.0 で演出なし
    float blinkSpeed_ = 9.0f;           // 危険時の明滅の速さ
    float blinkStrength_ = 0.45f;       // 危険時の明滅の強さ（0で明滅なし）

    // ─── 数値表示の設定 ────────────────────────────────────────
    Text valueText_{};                          // 残り秒数のテキスト
    std::string valueLabel_{"TIME "};           // 数字の前に付ける文字列
    Vector2 valuePosition_{32.0f, 88.0f};       // テキスト左上のピクセル座標
    float valueFontSize_ = 40.0f;               // 通常時のフォントサイズ
    float valuePunchScale_ = 1.35f;             // 時間が増えた瞬間に何倍まで大きくするか

    // ─── 「+○○s」ポップアップの設定 ───────────────────────────
    std::array<GainPopup, POPUP_COUNT> popups_{};   // 使い回すポップアップ（確保数は POPUP_COUNT）
    Vector2 popupPosition_{470.0f, 132.0f};         // ポップアップの出現位置
    float popupFontSize_ = 34.0f;                   // ポップアップのフォントサイズ
    Vector4 popupColor_{0.55f, 1.0f, 0.7f, 1.0f};   // ポップアップの色
    float popupRiseDistance_ = 52.0f;               // 消えるまでに何ピクセル上へ浮くか
    float popupDuration_ = 0.9f;                    // 表示してから消えるまでの秒数
    float popupStackOffset_ = 30.0f;                // 同時表示が重なったときに縦へずらす量

    // ─── 描画に使うスプライト ──────────────────────────────────
    Sprite frameSprite_{};      // 枠（＝時間が減った部分の下地）
    Sprite ghostSprite_{};      // 増えた分を先行表示する明るいバー
    Sprite fillSprite_{};       // 残り時間を表す本体バー
    Sprite flashSprite_{};      // 増えた瞬間に光らせる白いオーバーレイ

public:
    /// JSON からパラメータを読み込み、ゲージとテキストを初期化する
    void Initialize();

    /// 残り時間を減らし、UI と演出を更新する
    /// _deltaTime 前フレームからの経過秒数
    void Update(float _deltaTime);

    /// ゲージ・数値・ポップアップを描画キューへ積む
    /// スプライトを使うので、3D の描画がすべて終わったあとに呼ぶ
    void Draw();

    /// 残り時間を増やし、「増えた」ことが分かる演出を再生する
    /// _seconds 加算する秒数（0以下なら何もしない）
    /// 上限 maxSeconds_ で頭打ちになり、実際に増えた分だけが演出に反映される
    void AddTime(float _seconds);

    /// 残り時間を開始値へ戻す（リトライ時などに使用）
    void Reset();

    // ─── Getter / Setter ───────────────────────────────────────

    /// @brief 残り時間（秒）
    float GetRemainingSeconds() const { return remainingSeconds_; }

    /// @brief 時間切れかどうか
    bool IsTimeUp() const { return remainingSeconds_ <= 0.0f; }

    /// @brief 残り時間の割合（0〜1）。ゲージの見た目ではなく実値を返す
    float GetRemainingRatio() const;

    /// @brief カウントダウンの一時停止 / 再開
    void SetCountingDown(bool _countingDown) { countingDown_ = _countingDown; }

    /// @brief UI の表示 / 非表示を切り替える
    void SetVisible(bool _visible);

private:
    /// @brief Assets/Data/TimeLimit/TimeLimit.json から各パラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief ゲージの追従とタイマー類を進める
    void UpdateGauge(float _deltaTime);

    /// @brief ポップアップの浮き上がりとフェードを進める
    void UpdatePopups(float _deltaTime);

    /// @brief 現在の状態をスプライトの位置・サイズ・色へ反映する
    void ApplyGaugeSprites();

    /// @brief 残り秒数のテキストを更新する
    void RefreshValueText();

    /// @brief 空いているポップアップを1つ使って「+○○s」を表示する
    void SpawnGainPopup(float _seconds);

    /// @brief 残量に応じたゲージの基本色を返す（危険時の明滅もここで掛ける）
    Vector4 GetGaugeColor() const;

    /// @brief パンチスケールの現在倍率を返す（演出中でなければ 1.0）
    float GetPunchScale() const;

    /// @brief フラッシュの現在の濃度を返す（演出中でなければ 0.0）
    float GetFlashAlpha() const;
};

#endif // TIME_LIMIT_MANAGER_HPP_
