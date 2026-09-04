#ifndef MAIN_TOWER_HPP_
#define MAIN_TOWER_HPP_

#include "Tower.hpp"

/** 拠点となるメインタワー
 *
 *  通常のタワーとの違いは次の2点。
 *    - 敵との衝突判定が有効（敵はここへ向かって進んでくる）
 *    - HP を持ち、敵に到達されると減る。0 になったら IsDestroyed() が true になる
 *
 *  ## HP の扱い
 *  ダメージを与えるのは EnemyManager。敵1体が到達するたびに
 *  Enemy::GetTowerDamage() の値で TakeDamage() が呼ばれる。
 *  「1体あたり何ダメージか」は Assets/Data/Enemy/Enemy.json の "TowerDamage" で、
 *  「タワーの最大HP」は Assets/Data/Tower/MainTower.json の "Health" で変更できる。
 *
 *  UI 表示は TowerHpGauge が担当する。こちらはタワーの HP を読むだけなので、
 *  このクラスは UI の存在を知らない。
 */
class MainTower final : public Tower {
    std::unique_ptr<Model> baseModel_;
    std::unique_ptr<Collision::Collider> baseCollider_;

    // ─── HP ────────────────────────────────────────────────────
    float maxHp_ = 100.0f;              //!< 最大HP。ゲージ満タンの基準にもなる
    float hp_ = 100.0f;                 //!< 現在のHP

    // ─── 被弾時の見た目 ────────────────────────────────────────
    float damageFlashTimer_ = 0.0f;     //!< 被弾フラッシュの残り時間
    float damageFlashDuration_ = 0.3f;  //!< 被弾フラッシュの長さ（秒）
    Vector4 damageFlashColor_{1.0f, 0.3f, 0.25f, 1.0f}; //!< 被弾した瞬間に寄せる色
    bool hovered_ = false;              //!< マウスで選択中か（色の決定に使う）

public:
    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
    void SetHovered(bool _hovered) override;

    /// @brief タワーにダメージを与える
    /// @param _damage 与えるダメージ量（0以下・不正値なら何もしない）
    /// @note HP は 0 で下げ止まる。被弾したことが分かるようにモデルが一瞬光る
    void TakeDamage(float _damage);

    /// @brief タワーのHPを回復する（回復アイテムなどを足す場合に使う）
    /// @param _amount 回復量。最大HPで頭打ちになる
    void Heal(float _amount);

    /// @brief HP を最大値へ戻す（リトライ時などに使用）
    void ResetHp();

    /// @brief 現在のHP
    float GetHp() const { return hp_; }

    /// @brief 最大HP
    float GetMaxHp() const { return maxHp_; }

    /// @brief 残りHPの割合（0〜1）
    float GetHpRatio() const;

    /// @brief HP が尽きたか
    /// @note ゲームオーバー処理はまだ繋いでいない。判定はここを見るだけでよい
    bool IsDestroyed() const { return hp_ <= 0.0f; }

private:
    /// @brief Assets/Data/Tower/MainTower.json の "Health" グループを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 選択状態と被弾フラッシュからモデルの色を決めて反映する
    void ApplyModelColor();
};

#endif // MAIN_TOWER_HPP_
