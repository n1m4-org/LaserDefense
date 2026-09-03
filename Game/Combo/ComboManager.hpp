#ifndef COMBO_MANAGER_HPP_
#define COMBO_MANAGER_HPP_

#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

/** @brief 連続撃破（コンボ）の管理と UI 表示を受け持つクラス
 *
 *  ScoreManager / TimeLimitManager と同じく、責務を「コンボ数を数えて倍率を返す」ことに絞っている。
 *  スコアへの反映は EnemyManager 側で GetMultiplier() を掛けて行うため、
 *  このクラスはスコアの存在を知らない。
 *
 *  ## 仕様
 *  - 敵をレーザーで倒すと AddCombo() でコンボ数が1増え、継続時間がリセットされる
 *  - 最後の撃破から durationSeconds_ 秒経つとコンボが途切れる（時間式）
 *  - 敵をタワーに到達させても途切れる（ミス式）。EnemyManager から Break() が呼ばれる
 *  - 倍率は killsPerStep_ キルごとに1段上がり、maxMultiplier_ で頭打ち
 *      COMBO  0-4  -> x1 /  5-9 -> x2 / 10-14 -> x3 / ... / 35+ -> x8
 *
 *  ## 表示の狙い
 *  TimeLimitManager と同じ方針で、視線を向けなくても状態が分かるようにしている。
 *  - 倍率の段が上がった瞬間だけ大きめのパンチ＋ゲージのフラッシュ（＝「上がった」が分かる）
 *  - 継続ゲージが残り少なくなると赤く明滅（＝「切れる」が分かる）
 *  - 倍率が上がるほど文字とゲージの色が寒色→暖色へ変化（＝今どのくらい強いかが分かる）
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/Combo/Combo.json から上書きされる。
 *  継続時間・倍率の刻み・上限・配置・演出の強さは再ビルドせずに調整できる。
 *
 *  ## 使い方
 *  @code
 *      comboManager_ = std::make_unique<ComboManager>();
 *      comboManager_->Initialize();
 *      enemyManager_->SetComboManager(comboManager_.get());
 *
 *      comboManager_->Update(deltaTime);   // 毎フレーム
 *      comboManager_->Draw();              // 毎フレーム（3D描画のあとに呼ぶ）
 *  @endcode
 */
class ComboManager final {
    // ─── コンボのルール ────────────────────────────────────────
    int32_t comboCount_ = 0;            //!< 現在の連続撃破数
    float durationSeconds_ = 4.0f;      //!< 最後の撃破からコンボが切れるまでの秒数
    float remainingSeconds_ = 0.0f;     //!< コンボが切れるまでの残り秒数
    int32_t killsPerStep_ = 5;          //!< 何キルごとに倍率を1段上げるか
    int32_t maxMultiplier_ = 8;         //!< 倍率の上限
    bool breakOnTowerReach_ = true;     //!< 敵にタワーへ到達されたらコンボを切るか

    // ─── 演出用タイマー ────────────────────────────────────────
    float punchTimer_ = 0.0f;           //!< 倍率テキストのパンチスケールの残り時間
    float flashTimer_ = 0.0f;           //!< 段が上がったときのフラッシュの残り時間
    float blinkTime_ = 0.0f;            //!< 切れかけの明滅に使う経過時間
    bool tierUp_ = false;               //!< 現在のパンチが「段が上がった」ものか

    // ─── 演出の設定 ────────────────────────────────────────────
    float punchDuration_ = 0.28f;       //!< パンチスケールの長さ（秒）
    float punchScale_ = 1.35f;          //!< 通常の撃破で膨らむ倍率
    float tierUpPunchScale_ = 1.9f;     //!< 段が上がったときに膨らむ倍率（より大きく）
    float flashDuration_ = 0.3f;        //!< 段が上がったときのフラッシュの長さ（秒）
    float flashStrength_ = 0.9f;        //!< フラッシュの最大濃度（0〜1）
    float warningRatio_ = 0.3f;         //!< 継続ゲージがこの割合を切ると明滅して警告する
    float blinkSpeed_ = 12.0f;          //!< 警告の明滅の速さ
    float blinkStrength_ = 0.5f;        //!< 警告の明滅の強さ（0で明滅なし）

    // ─── 倍率テキスト（"x3"）の設定 ────────────────────────────
    Text multiplierText_{};
    Vector2 multiplierPosition_{32.0f, 170.0f};         //!< テキスト左上のピクセル座標
    float multiplierFontSize_ = 46.0f;                  //!< 通常時のフォントサイズ
    Vector4 lowTierColor_{0.85f, 0.95f, 1.0f, 1.0f};    //!< 倍率が低いときの色（寒色）
    Vector4 highTierColor_{1.0f, 0.45f, 0.15f, 1.0f};   //!< 倍率が上限のときの色（暖色）

    // ─── コンボ数テキスト（"COMBO 12"）の設定 ──────────────────
    Text countText_{};
    std::string countLabel_{"COMBO "};                  //!< 数字の前に付ける文字列
    Vector2 countPosition_{104.0f, 186.0f};             //!< テキスト左上のピクセル座標
    float countFontSize_ = 28.0f;                       //!< フォントサイズ
    Vector4 countColor_{0.8f, 0.85f, 0.9f, 1.0f};       //!< 文字色

    // ─── 継続ゲージの設定 ──────────────────────────────────────
    Sprite gaugeFrameSprite_{};     //!< 枠（＝減った部分の下地）
    Sprite gaugeFillSprite_{};      //!< 残り継続時間を表すバー
    Sprite gaugeFlashSprite_{};     //!< 段が上がった瞬間に光らせる白いオーバーレイ
    Vector2 gaugePosition_{32.0f, 232.0f};              //!< ゲージ左端・中心のピクセル座標
    Vector2 gaugeSize_{220.0f, 6.0f};                   //!< ゲージ本体のサイズ（満タン時）
    float gaugeFrameThickness_ = 2.0f;                  //!< ゲージを囲む枠の太さ
    Vector4 gaugeFrameColor_{0.04f, 0.04f, 0.07f, 0.85f};   //!< 枠の色

    bool visible_ = true;           //!< UI 全体の表示 / 非表示

public:
    /// @brief JSON からパラメータを読み込み、テキストとゲージを初期化する
    void Initialize();

    /// @brief 継続時間を減らし、UI と演出を更新する
    /// @param _deltaTime 前フレームからの経過秒数
    /// @note 継続時間が尽きたらこの中でコンボが途切れる
    void Update(float _deltaTime);

    /// @brief コンボ UI を描画キューへ積む
    /// @note スプライトを使うので、3D の描画がすべて終わったあとに呼ぶ。
    ///       コンボが 0 のときは何も描かない
    void Draw();

    /// @brief 連続撃破数を1増やし、継続時間をリセットする
    /// @note 倍率の段が上がった場合は、より強い演出が再生される
    void AddCombo();

    /// @brief コンボを途切れさせる（タワーへ到達された場合など）
    /// @note breakOnTowerReach_ が false のときは EnemyManager から呼ばれない
    void Break();

    /// @brief コンボを初期状態へ戻す（リトライ時などに使用）
    void Reset();

    // ─── Getter / Setter ───────────────────────────────────────

    /// @brief 現在の連続撃破数
    int32_t GetCount() const { return comboCount_; }

    /// @brief 現在のスコア倍率
    /// @return killsPerStep_ キルごとに1段上がり、maxMultiplier_ で頭打ちになった倍率（最低1）
    int32_t GetMultiplier() const;

    /// @brief コンボが継続中か
    bool IsActive() const { return comboCount_ > 0; }

    /// @brief コンボが切れるまでの残り割合（0〜1）
    float GetRemainingRatio() const;

    /// @brief タワー到達でコンボを切る設定か
    bool IsBreakOnTowerReach() const { return breakOnTowerReach_; }

    /// @brief UI の表示 / 非表示を切り替える
    void SetVisible(bool _visible) { visible_ = _visible; }

private:
    /// @brief Assets/Data/Combo/Combo.json から各パラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 現在の状態をスプライトの位置・サイズ・色へ反映する
    void ApplyGaugeSprites();

    /// @brief 倍率とコンボ数のテキストを更新する
    void RefreshTexts();

    /// @brief 倍率の高さに応じた色を返す（切れかけの明滅もここで掛ける）
    Vector4 GetTierColor() const;

    /// @brief パンチスケールの現在倍率を返す（演出中でなければ 1.0）
    float GetPunchScale() const;

    /// @brief フラッシュの現在の濃度を返す（演出中でなければ 0.0）
    float GetFlashAlpha() const;
};

#endif // COMBO_MANAGER_HPP_
