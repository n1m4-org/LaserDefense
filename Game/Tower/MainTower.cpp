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
    constexpr Vector4 SELECTION_COLOR{0.1f, 0.35f, 1.0f, 0.8f};
    constexpr Vector4 CONNECTED_COLOR{0.15f, 0.8f, 1.0f, 1.0f};

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
    hp_ = maxHp_;

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
    Tower::Update(_deltaTime);

    // 被弾フラッシュを減衰させ、色へ反映する
    damageFlashTimer_ = std::max(damageFlashTimer_ - _deltaTime, 0.0f);
    ApplyModelColor();

    const Vector3 center = GetPosition() + Vector3{0.0f, 1.0f, 0.0f};
    baseModel_->SetTranslate(center);
    baseModel_->Update();
    if (hovered_) {
        baseSelectionModel_->SetTranslate(center);
        baseSelectionModel_->SetScale(Vector3{2.5f, 1.0f, 2.5f} * GetSelectionScaleMultiplier());
        baseSelectionModel_->Update();
    }
    baseCollider_->SetTranslate(center + GetColliderOffset());
}

void MainTower::Draw() {
    baseModel_->Draw();
    if (hovered_ && baseSelectionModel_) baseSelectionModel_->Draw();
    Tower::Draw();
}

void MainTower::SetHovered(bool _hovered) {
    Tower::SetHovered(_hovered);
    ApplyModelColor();
}

void MainTower::SetConnected(bool _connected) {
    connected_ = _connected;
    ApplyModelColor();
}

Vector3 MainTower::GetSelectionCenter() const {
    // 高さ2の土台と、その上に立つ高さ10の柱をまとめた中心。
    return GetPosition() + Vector3{0.0f, 6.0f, 0.0f} + GetColliderOffset();
}

Vector3 MainTower::GetSelectionSize() const {
    return {5.0f, 12.0f, 5.0f};
}

void MainTower::TakeDamage(float _damage) {
    if (!std::isfinite(_damage) || _damage <= 0.0f) {
        return;
    }

    hp_ = std::max(hp_ - _damage, 0.0f);
    // タワー本体も光らせる。UI を見ていなくても「拠点が殴られた」ことが分かるようにする
    damageFlashTimer_ = damageFlashDuration_;
    ApplyModelColor();
}

void MainTower::Heal(float _amount) {
    if (!std::isfinite(_amount) || _amount <= 0.0f) {
        return;
    }
    hp_ = std::min(hp_ + _amount, maxHp_);
}

void MainTower::ResetHp() {
    hp_ = maxHp_;
    damageFlashTimer_ = 0.0f;
    ApplyModelColor();
}

float MainTower::GetHpRatio() const {
    if (maxHp_ <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(hp_ / maxHp_, 0.0f, 1.0f);
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
        maxHp_ = read(health->second, "MaxHp", maxHp_);
        damageFlashDuration_ = read(health->second, "DamageFlashDuration", damageFlashDuration_);
        damageFlashColor_ = read(health->second, "DamageFlashColor", damageFlashColor_);
    }

    // 不正な値が入っていても破綻しないように補正する
    maxHp_ = std::max(maxHp_, 1.0f);
    damageFlashDuration_ = std::max(damageFlashDuration_, 0.0f);
}

void MainTower::ApplyModelColor() {
    // 被弾直後ほど damageFlashColor_ へ強く寄せ、時間とともに元の色へ戻す
    float flash = 0.0f;
    if (damageFlashTimer_ > 0.0f && damageFlashDuration_ > 0.0f) {
        flash = EaseOutCubic(damageFlashTimer_ / damageFlashDuration_);
    }

    const Vector4 pillarBaseColor = connected_ ? CONNECTED_COLOR : PILLAR_NORMAL_COLOR;
    const Vector4 baseBaseColor = connected_ ? CONNECTED_COLOR : BASE_NORMAL_COLOR;
    // 選択表現は半透明モデルへ分離し、本体色は接続状態と被弾フラッシュを扱う。
    if (model_) {
        model_->SetColor(LerpColor(pillarBaseColor, damageFlashColor_, flash));
    }
    if (baseModel_) {
        baseModel_->SetColor(LerpColor(baseBaseColor, damageFlashColor_, flash));
    }
}
