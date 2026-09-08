#define NOMINMAX

#include "MainTower.hpp"

#include <algorithm>
#include <cmath>

#include "Collision/CollisionAttribute.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// 通常時の色（柱／土台）
    constexpr Vector4 PILLAR_NORMAL_COLOR{0.0f, 1.0f, 0.0f, 1.0f};
    constexpr Vector4 BASE_NORMAL_COLOR{0.2f, 0.6f, 0.2f, 1.0f};
    constexpr Vector4 SUB_TOWER_COLOR{0.0f, 1.0f, 0.0f, 1.0f};
    constexpr Vector4 SELECTION_COLOR{0.1f, 0.35f, 1.0f, 0.8f};
    constexpr Vector4 CONNECTED_COLOR{0.15f, 0.8f, 1.0f, 1.0f};
    // Modelの影登録は通常描画と独立しているため、完全に非表示の柱は影響範囲外へ退避する。
    constexpr float HIDDEN_PILLAR_DROP = 1000.0f;

    /// 終わり際がゆっくりになる補間（フラッシュの減衰に使う）
    float EaseOutCubic(float _t) {
        const float inv = 1.0f - _t;
        return 1.0f - inv * inv * inv;
    }

    /// 色の線形補間
    Vector4 LerpColor(const Vector4& _start, const Vector4& _end, float _t) {
        return {
            _start.x + (_end.x - _start.x) * _t,
            _start.y + (_end.y - _start.y) * _t,
            _start.z + (_end.z - _start.z) * _t,
            _start.w + (_end.w - _start.w) * _t,
        };
    }
}

void MainTower::Initialize() {
    Tower::Initialize();
    // メインタワーの柱は、通常タワーが低い形状になっても従来の縦長を維持する。
    SetScale({1.0f, 5.0f, 1.0f});
    LoadConfig();

    // 通常タワーは敵を無視するが、メインタワーの柱は判定を有効にする。
    // 土台のコライダーも従来どおりEnemyを無視しない。
    SetEnemyCollisionEnabled(true);
    // 土台は幅5・高さ2。高さ10の柱をその上に置く。
    modelOffset_ = {0.0f, 7.0f, 0.0f};
    baseModel_ = std::make_unique<Model>();
    baseModel_->Initialize("Cube");
    baseModel_->SetEnvironmentTexture("skybox.dds");
    baseModel_->SetScale({2.5f, 1.0f, 2.5f});
    baseModel_->SetColor(BASE_NORMAL_COLOR);
    baseSelectionModel_ = std::make_unique<Model>();
    baseSelectionModel_->Initialize("Cube");
    baseSelectionModel_->SetEnvironmentTexture("skybox.dds");
    baseSelectionModel_->SetColor(SELECTION_COLOR);
    baseCollider_ = std::make_unique<Collision::Collider>();
    baseCollider_->SetName("MainTowerBase")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(static_cast<Tower*>(this))
        ->AddAttribute(CollisionAttribute::Tower)
        ->AddIgnore(CollisionAttribute::Tower)
        ->SetSize(Vector3{5.0f, 2.0f, 5.0f})
        ->Enable();
    SetHovered(false);
}

void MainTower::Update(float _deltaTime) {
    if (std::isfinite(_deltaTime) && _deltaTime > 0.0f) {
        transitionTime_ = std::min(transitionTime_ + _deltaTime, transitionDuration_);
    }
    const float t = std::clamp(transitionTime_ / transitionDuration_, 0.0f, 1.0f);
    const float eased = defenseTarget_ ? EaseOutCubic(t) : t * t * t;
    const float targetRatio = defenseTarget_ ? 1.0f : 0.0f;
    pillarHeightRatio_ = transitionStartRatio_ + (targetRatio - transitionStartRatio_) * eased;

    Tower::Update(_deltaTime);
    // 見た目だけを上下させ、当たり判定と接続先の座標は変えない。
    const bool pillarHidden = !defenseTarget_ && !warningActive_
        && pillarHeightRatio_ <= 0.0001f;
    const float pillarDrop = pillarHidden
        ? HIDDEN_PILLAR_DROP
        : 12.0f * (1.0f - (warningActive_ && !defenseTarget_ ? 1.0f : pillarHeightRatio_));
    model_->SetTranslate(GetPosition() + modelOffset_
        + Vector3{0.0f, -pillarDrop, 0.0f});

    // 選択用モデルも影描画へ常時登録されるため、通常描画しない状態では地下へ退避する。
    // Tower::Update()がマウスオーバー中のselectionModel_を更新した後に判定し直すことで、
    // 非メインタワーを選択したときや、マウスを外した後に影だけ残る状態を防ぐ。
    const bool showPillarSelection = defenseTarget_
        && transitionTime_ >= transitionDuration_ && hovered_;
    if (selectionModel_ && !showPillarSelection) {
        selectionModel_->SetTranslate(
            GetPosition() + Vector3{0.0f, -HIDDEN_PILLAR_DROP, 0.0f});
        selectionModel_->Update();
    }

    // 被弾フラッシュを減衰させ、色へ反映する
    damageFlashTimer_ = std::max(damageFlashTimer_ - _deltaTime, 0.0f);
    ApplyModelColor();
    model_->Update();

    const Vector3 center = GetPosition() + Vector3{0.0f, 1.0f, 0.0f};
    baseModel_->SetTranslate(center);
    baseModel_->Update();
    if (hovered_) {
        baseSelectionModel_->SetTranslate(center);
        baseSelectionModel_->SetScale(Vector3{2.5f, 1.0f, 2.5f} * GetSelectionScaleMultiplier());
        baseSelectionModel_->Update();
    } else {
        baseSelectionModel_->SetTranslate(
            GetPosition() + Vector3{0.0f, -HIDDEN_PILLAR_DROP, 0.0f});
        baseSelectionModel_->Update();
    }
    baseCollider_->SetTranslate(center + GetColliderOffset());
}

void MainTower::Draw() {
    baseModel_->Draw();
    if (hovered_ && baseSelectionModel_) baseSelectionModel_->Draw();
    // サブタワー時は土台だけを表示し、メイン化したときだけ柱を追加する。
    if (pillarHeightRatio_ > 0.0f || warningActive_) model_->Draw();
    if (defenseTarget_ && transitionTime_ >= transitionDuration_ && hovered_ && selectionModel_) {
        selectionModel_->Draw();
    }
}

void MainTower::SetHovered(bool _hovered) {
    Tower::SetHovered(_hovered);
    ApplyModelColor();
}

void MainTower::SetConnected(bool _connected) {
    connected_ = _connected;
    ApplyModelColor();
}

void MainTower::SetDefenseTarget(bool _enabled, bool _animate) {
    if (defenseTarget_ != _enabled) {
        transitionDuration_ = _enabled ? appearanceDuration_ : disappearanceDuration_;
        transitionStartRatio_ = pillarHeightRatio_;
        transitionTime_ = _animate ? 0.0f : transitionDuration_;
        if (!_animate) pillarHeightRatio_ = _enabled ? 1.0f : 0.0f;
    }
    defenseTarget_ = _enabled;
    SetColliderEnabled(_enabled);
    SetEnemyCollisionEnabled(_enabled);
    if (baseCollider_) {
        if (_enabled) baseCollider_->RemoveIgnore(CollisionAttribute::Enemy);
        else baseCollider_->AddIgnore(CollisionAttribute::Enemy);
    }
    ApplyModelColor();
}

void MainTower::SetSwitchWarningProgress(float _progress) {
    warningActive_ = _progress >= 0.0f;
    warningOpacity_ = _progress < 0.0f ? 1.0f
        : switchWarningAlpha_ + (switchWarningMaxAlpha_ - switchWarningAlpha_) * _progress;
    ApplyModelColor();
}

Vector3 MainTower::GetSelectionCenter() const {
    if (!defenseTarget_) {
        return GetPosition() + Vector3{0.0f, 1.0f, 0.0f} + GetColliderOffset();
    }
    // 高さ2の土台と、その上に立つ高さ10の柱をまとめた中心。
    return GetPosition() + Vector3{0.0f, 6.0f, 0.0f} + GetColliderOffset();
}

Vector3 MainTower::GetSelectionSize() const {
    return defenseTarget_ ? Vector3{5.0f, 12.0f, 5.0f} : Vector3{5.0f, 2.0f, 5.0f};
}

void MainTower::PlayDamageFlash() {
    damageFlashTimer_ = damageFlashDuration_;
    ApplyModelColor();
}

void MainTower::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Tower", "MainTower")) {
        return;
    }

    const auto groups = json->GetGroups("MainTower");

    // キーが存在し、かつ想定した型で入っている場合だけ値を取り出すヘルパー
    const auto read = []<typename T>(const auto& _group, const std::string& _key, const T& _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) {
            return _fallback;
        }
        if (const auto value = std::get_if<T>(&entry->second)) {
            return *value;
        }
        return _fallback;
    };

    if (const auto health = groups.find("Health"); health != groups.end()) {
        damageFlashDuration_ = read(health->second, "DamageFlashDuration", damageFlashDuration_);
        damageFlashColor_ = read(health->second, "DamageFlashColor", damageFlashColor_);
    }

    if (const auto switchConfig = groups.find("Switch"); switchConfig != groups.end()) {
        switchWarningAlpha_ = read(
            switchConfig->second, "WarningAlpha", switchWarningAlpha_);
        switchWarningMaxAlpha_ = read(switchConfig->second, "WarningMaxAlpha", switchWarningMaxAlpha_);
        appearanceDuration_ = read(switchConfig->second, "AppearanceSeconds", appearanceDuration_);
        disappearanceDuration_ = read(switchConfig->second, "TransitionSeconds", disappearanceDuration_);
    }

    switchWarningMaxAlpha_ = std::isfinite(switchWarningMaxAlpha_)
        ? std::clamp(switchWarningMaxAlpha_, 0.0f, 1.0f) : 0.5f;
    appearanceDuration_ = std::isfinite(appearanceDuration_)
        ? std::max(appearanceDuration_, 0.01f) : 0.5f;
    disappearanceDuration_ = std::isfinite(disappearanceDuration_)
        ? std::max(disappearanceDuration_, 0.01f) : 0.2f;
    // 不正な値が入っていても破綻しないように補正する
    switchWarningAlpha_ = std::isfinite(switchWarningAlpha_) ? std::clamp(switchWarningAlpha_, 0.0f, 1.0f) : 0.0f;
    damageFlashDuration_ = std::max(damageFlashDuration_, 0.0f);
}

void MainTower::ApplyModelColor() {
    // 被弾直後ほど damageFlashColor_ へ強く寄せ、時間とともに元の色へ戻す
    float flash = 0.0f;
    if (damageFlashTimer_ > 0.0f && damageFlashDuration_ > 0.0f) {
        flash = EaseOutCubic(damageFlashTimer_ / damageFlashDuration_);
    }

    const Vector4 pillarBaseColor = connected_ ? CONNECTED_COLOR : PILLAR_NORMAL_COLOR;
    const Vector4 normalBaseColor = defenseTarget_ ? BASE_NORMAL_COLOR : SUB_TOWER_COLOR;
    const Vector4 baseBaseColor = connected_ ? CONNECTED_COLOR : normalBaseColor;
    // 選択表現は半透明モデルへ分離し、本体色は接続状態と被弾フラッシュを扱う。
    if (model_) {
        Vector4 pillarColor = LerpColor(pillarBaseColor, damageFlashColor_, flash);
        pillarColor.w *= warningOpacity_;
        model_->SetColor(pillarColor);
    }
    if (baseModel_) {
        baseModel_->SetColor(LerpColor(baseBaseColor, damageFlashColor_, flash));
    }
}
