#ifndef MAIN_TOWER_HPP_
#define MAIN_TOWER_HPP_

#include <optional>

#include "Tower.hpp"

/// 防衛対象の表示と衝突を担当する。共有HPはTowerManagerが所有する。
class MainTower final : public Tower {
    std::unique_ptr<Model> baseModel_;
    std::unique_ptr<Model> baseSelectionModel_;
    std::unique_ptr<Collision::Collider> baseCollider_;

    // ─── 被弾時の見た目 ────────────────────────────────────────
    float damageFlashTimer_ = 0.0f;     //!< 被弾フラッシュの残り時間
    float damageFlashDuration_ = 0.3f;  //!< 被弾フラッシュの長さ（秒）
    Vector4 damageFlashColor_{1.0f, 0.3f, 0.25f, 1.0f}; //!< 被弾した瞬間に寄せる色
    bool defenseTarget_ = true;         //!< 現在、防衛対象のメインタワーとして有効か
    std::optional<Vector4> colorOverride_; //!< 設定されている間、通常の状態色より優先して使う色
    bool warningActive_ = false;
    float warningOpacity_ = 1.0f;
    float switchWarningAlpha_ = 0.0f;
    float switchWarningMaxAlpha_ = 0.5f;
    float appearanceDuration_ = 0.5f;
    float disappearanceDuration_ = 0.2f;
    float transitionDuration_ = 0.5f;
    float transitionTime_ = 0.2f;
    float pillarHeightRatio_ = 1.0f;
    float transitionStartRatio_ = 1.0f;

public:
    void Initialize() override;
    void Update(float _deltaTime) override;
    void Draw() override;
    void SetHovered(bool _hovered) override;
    void SetConnected(bool _connected) override;
    void SetDefenseTarget(bool _enabled, bool _animate = true);
    void SetSwitchWarningProgress(float _progress);
    bool IsDefenseTarget() const { return defenseTarget_; }

    /// @brief 通常の状態色より優先して使う色を設定する
    /// @param _color 上書きする色。std::nulloptを渡すと通常の状態色に戻る
    /// @note タワー自身は上書き理由を関知しない、汎用的な見た目のフックである
    void SetColorOverride(const std::optional<Vector4>& _color);
    Vector3 GetSelectionCenter() const override;
    Vector3 GetSelectionSize() const override;

    void PlayDamageFlash();

private:
    /// @brief Assets/Data/Tower/MainTower.json の "Health" グループを読み込む
    /// @note ファイルやキーが無い場合はメンバーの初期値がそのまま使われる
    void LoadConfig();

    /// @brief 選択状態と被弾フラッシュからモデルの色を決めて反映する
    void ApplyModelColor();
};

#endif // MAIN_TOWER_HPP_
