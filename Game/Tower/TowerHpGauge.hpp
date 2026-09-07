#ifndef TOWER_HP_GAUGE_HPP_
#define TOWER_HP_GAUGE_HPP_

#include <array>
#include <cstdint>
#include <string>

#include "Math/Vector2.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"
#include "Text/Text.hpp"

class MainTower;

/** メインタワーの HP を常時表示するゲージ UI
 *
 *  HP の実データは MainTower が持っており、このクラスは毎フレームそれを読むだけ。
 *  HP が減ったことは「前フレームとの差」で検知するので、
 *  MainTower 側に UI への通知を書く必要がない（＝タワーは UI を知らない）。
 *
 *  ## 画面のどこに置いているか
 *  画面上端の中央に横長で置いている。理由は次のとおり。
 *    - メインタワーはステージ中央にあり、プレイヤーの視線も画面中央にある。
 *      中央上はそこから視線移動が最も短く、周辺視野にも入る
 *    - 「負け条件そのもののHPは画面中央に横長」というボス戦 UI の慣習に沿う
 *    - 左上（制限時間）と右上（スコア）は既に埋まっており、役割ごとに三分割できる
 *
 *  ## 表示の狙い
 *  SurvivalTimeManager / ScoreManager と同じ演出言語で揃えている。
 *    1. トレイルバー … 失った分がその場に残ってから遅れて減る（どれだけ減ったかが分かる）
 *    2. フラッシュ   … 被弾した瞬間にゲージ全体が白く光る（輝度の急変）
 *    3. シェイク     … ゲージが左右に揺れる（動き）
 *    4. 画面フラッシュ … 画面全体がうっすら赤く光る（視線がどこにあっても気付ける）
 *    5. 危険域の明滅 … 残りが少ないとゲージが赤く明滅し続ける
 *  制限時間ゲージが「増える」方向の演出なのに対し、こちらは「減る」方向なので、
 *  色を寒色（水色）にして左上の時間ゲージと一目で区別できるようにしている。
 *
 *  ## パラメータの変更方法
 *  下記メンバーの初期値はすべて Assets/Data/Tower/MainTower.json から上書きされる。
 *  配置・色・演出の強さは再ビルドせずに JSON の値だけで調整できる。
 *
 *  ## 使い方
 *  @code
 *      towerHpGauge_ = std::make_unique<TowerHpGauge>();
 *      towerHpGauge_->Initialize();
 *      towerHpGauge_->SetTarget(mainTower);    // 表示したいタワーを渡す
 *
 *      towerHpGauge_->Update(deltaTime);       // 毎フレーム
 *      towerHpGauge_->Draw();                  // 毎フレーム（3D描画のあとに呼ぶ）
 *  @endcode
 */
class TowerHpGauge final {
    /// 同時に表示できる「-○○」ポップアップの最大数
    static constexpr size_t POPUP_COUNT = 4;

    /// 「-○○」ポップアップ1個分の状態
    struct DamagePopup {
        Text text{};              // 表示用テキスト
        float elapsed = 0.0f;     // 表示開始からの経過秒数
        float baseX = 0.0f;       // 中央揃えで決まった X 座標
        float baseY = 0.0f;       // 落ち始めの起点になる Y 座標
        bool active = false;      // 表示中かどうか
    };

    // ─── 表示対象 ──────────────────────────────────────────────
    const MainTower* target_ = nullptr; //!< HP を読むタワー（所有権は持たない）
    float lastHp_ = -1.0f;              //!< 前フレームの HP。差分でダメージを検知する

    // ─── ゲージの表示状態 ──────────────────────────────────────
    float displayRatio_ = 1.0f;         //!< 本体バーの割合（実 HP をそのまま映す）
    float trailRatio_ = 1.0f;           //!< 失った分を残すトレイルバーの割合
    float trailStartRatio_ = 1.0f;      //!< トレイルが減り始めたときの割合
    float trailHoldTimer_ = 0.0f;       //!< 減り始めるまでの待ち時間の残り
    float trailTimer_ = 0.0f;           //!< 減っている最中の残り時間
    float trailHoldSeconds_ = 0.25f;    //!< 被弾後、トレイルがその場に留まる秒数
    float trailDrainSeconds_ = 0.45f;   //!< トレイルが本体バーへ追いつくまでの秒数

    // ─── 演出用タイマー ────────────────────────────────────────
    float flashTimer_ = 0.0f;           //!< フラッシュの残り時間
    float punchTimer_ = 0.0f;           //!< パンチスケールの残り時間
    float shakeTimer_ = 0.0f;           //!< シェイクの残り時間
    float blinkTime_ = 0.0f;            //!< 危険時の明滅に使う経過時間

    // ─── ゲージの見た目設定 ────────────────────────────────────
    Vector2 gaugePosition_{420.0f, 52.0f};      //!< ゲージ左端・中心のピクセル座標
    Vector2 gaugeSize_{440.0f, 22.0f};          //!< ゲージ本体のサイズ（満タン時）
    float frameThickness_ = 3.0f;               //!< ゲージを囲む枠の太さ
    Vector4 frameColor_{0.04f, 0.04f, 0.07f, 0.85f};    //!< 枠 兼 空き部分の色
    Vector4 safeColor_{0.35f, 0.8f, 1.0f, 1.0f};        //!< HP が十分あるときの色（寒色）
    Vector4 warningColor_{1.0f, 0.7f, 0.25f, 1.0f};     //!< HP が減ってきたときの色
    Vector4 dangerColor_{1.0f, 0.3f, 0.3f, 1.0f};       //!< HP が危険なときの色
    Vector4 trailColor_{1.0f, 1.0f, 1.0f, 0.95f};       //!< 失った分を示すトレイルバーの色
    float warningRatio_ = 0.4f;         //!< この割合を下回ると warningColor_ になる
    float dangerRatio_ = 0.2f;          //!< この割合を下回ると dangerColor_ になり明滅する

    // ─── 「被弾した瞬間」の演出設定 ────────────────────────────
    float flashDuration_ = 0.25f;       //!< フラッシュの長さ（秒）
    float flashStrength_ = 0.9f;        //!< フラッシュの最大濃度（0〜1）
    float punchDuration_ = 0.3f;        //!< パンチスケールの長さ（秒）
    float punchScale_ = 1.45f;          //!< 膨らむ倍率。1.0 で演出なし
    float shakeDuration_ = 0.3f;        //!< シェイクの長さ（秒）
    float shakeStrength_ = 7.0f;        //!< シェイクの振れ幅（ピクセル）
    float shakeSpeed_ = 42.0f;          //!< シェイクの速さ
    float blinkSpeed_ = 8.0f;           //!< 危険時の明滅の速さ
    float blinkStrength_ = 0.45f;       //!< 危険時の明滅の強さ（0で明滅なし）
    Vector4 screenFlashColor_{1.0f, 0.1f, 0.1f, 1.0f};  //!< 画面全体を覆うフラッシュの色
    float screenFlashStrength_ = 0.22f; //!< 画面フラッシュの最大濃度（0で無効）

    // ─── ラベル（"TOWER"）の設定 ───────────────────────────────
    Text labelText_{};
    std::string label_{"TOWER"};                        //!< ゲージの上に出す見出し
    Vector2 labelPosition_{420.0f, 18.0f};              //!< テキスト左上のピクセル座標
    float labelFontSize_ = 22.0f;                       //!< フォントサイズ
    Vector4 labelColor_{0.75f, 0.8f, 0.88f, 1.0f};      //!< 文字色

    // ─── 数値（"100 / 100"）の設定 ─────────────────────────────
    // ラベルと同じ行に、ゲージの右端へ揃えて置く。
    // ゲージの下に置くとタワーのモデルと重なって読みにくくなるため
    Text valueText_{};
    std::string valueSeparator_{" / "};     //!< 現在値と最大値の間に挟む文字列
    float valueRightX_ = 860.0f;            //!< 右揃えの基準になる右端 X 座標
    float valuePositionY_ = 14.0f;          //!< テキスト上端の Y 座標
    float valueFontSize_ = 24.0f;           //!< 通常時のフォントサイズ
    float valuePunchScale_ = 1.3f;          //!< 被弾した瞬間に何倍まで大きくするか
    float charWidthRatio_ = 0.53f;          //!< 右揃えに使う「1文字幅 ÷ フォントサイズ」の目安

    // ─── 「-○○」ポップアップの設定 ───────────────────────────
    // ゲージの真下に中央揃えで出し、上ではなく「下へ落として」消す。
    // スコアや制限時間の "+N" が上へ浮くので、下向きにすることで
    // 増えたのか減ったのかを動きの向きだけで区別できる
    std::array<DamagePopup, POPUP_COUNT> popups_{};     //!< 使い回すポップアップ
    float popupCenterX_ = 640.0f;                       //!< 中央揃えの基準になる X 座標
    float popupPositionY_ = 112.0f;                     //!< ポップアップの出現 Y 座標
    float popupFontSize_ = 32.0f;                       //!< フォントサイズ
    Vector4 popupColor_{1.0f, 0.42f, 0.38f, 1.0f};      //!< 文字色
    float popupDropDistance_ = 40.0f;                   //!< 消えるまでに何ピクセル下へ落ちるか
    float popupDuration_ = 0.85f;                       //!< 表示してから消えるまでの秒数
    float popupStackOffset_ = 30.0f;                    //!< 同時表示が重なったときに縦へずらす量

    // ─── 描画に使うスプライト ──────────────────────────────────
    Sprite frameSprite_{};          //!< 枠（＝HP が減った部分の下地）
    Sprite trailSprite_{};          //!< 失った分を遅れて減らすバー
    Sprite fillSprite_{};           //!< 残り HP を表す本体バー
    Sprite flashSprite_{};          //!< 被弾した瞬間に光らせる白いオーバーレイ
    Sprite screenFlashSprite_{};    //!< 画面全体を覆う被弾フラッシュ

    bool visible_ = true;           //!< UI 全体の表示 / 非表示

public:
    /// @brief JSON からパラメータを読み込み、ゲージとテキストを初期化する
    void Initialize();

    /// @brief 表示するタワーを設定する
    /// @param _tower HP を読むメインタワー（所有権は持たない）
    /// @note 未設定（nullptr）の間はゲージを描画しない
    void SetTarget(const MainTower* _tower);

    /// @brief タワーの HP を読み取り、UI と演出を更新する
    /// @param _deltaTime 前フレームからの経過秒数
    void Update(float _deltaTime);

    /// @brief ゲージ・数値・ポップアップを描画キューへ積む
    /// @note スプライトを使うので、3D の描画がすべて終わったあとに呼ぶ
    void Draw();

    /// @brief 表示状態を初期化する（リトライ時などに使用）
    void Reset();

    /// @brief UI の表示 / 非表示を切り替える
    void SetVisible(bool _visible);

private:
    /// @brief Assets/Data/Tower/MainTower.json から各パラメータを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 被弾を検知して演出を起動する
    /// @param _damage 減った HP 量
    void OnDamaged(float _damage);

    /// @brief トレイルバーの追従とタイマー類を進める
    void UpdateGauge(float _deltaTime);

    /// @brief ポップアップの浮き上がりとフェードを進める
    void UpdatePopups(float _deltaTime);

    /// @brief 現在の状態をスプライトの位置・サイズ・色へ反映する
    void ApplyGaugeSprites();

    /// @brief 残り HP のテキストを更新する
    void RefreshValueText();

    /// @brief 空いているポップアップを1つ使って「-○○」を表示する
    void SpawnDamagePopup(float _damage);

    /// @brief 残量に応じたゲージの基本色を返す（危険時の明滅もここで掛ける）
    Vector4 GetGaugeColor() const;

    /// @brief シェイクによる X 方向のずれを返す（演出中でなければ 0.0）
    float GetShakeOffset() const;

    /// @brief パンチスケールの現在倍率を返す（演出中でなければ 1.0）
    float GetPunchScale() const;

    /// @brief フラッシュの現在の濃度を返す（演出中でなければ 0.0）
    float GetFlashAlpha() const;

    /// @brief 文字列の描画幅を概算する（Text に右揃え機能が無いため自前で見積もる）
    float EstimateTextWidth(const std::string& _text, float _fontSize) const;
};

#endif // TOWER_HP_GAUGE_HPP_
