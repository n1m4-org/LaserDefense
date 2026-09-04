#define NOMINMAX

#include "MainTower.hpp"

#include <algorithm>
#include <cmath>

#include "Collision/CollisionAttribute.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

namespace {
    /// 通常時の色（柱／土台）と、選択中の色
    constexpr Vector4 PILLAR_NORMAL_COLOR{0.0f, 1.0f, 0.0f, 1.0f};
    constexpr Vector4 BASE_NORMAL_COLOR{0.2f, 0.6f, 0.2f, 1.0f};
    constexpr Vector4 HOVERED_COLOR{1.0f, 0.0f, 0.0f, 1.0f};

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
    LoadConfig();
    hp_ = maxHp_;

    // 通常タワーは敵を無視するが、メインタワーの柱は判定を有効にする。
    // 土台のコライダーも従来どおりEnemyを無視しない。
    SetEnemyCollisionEnabled(true);
    // 土台は幅6・高さ2。通常タワーと同じ高さ10の柱をその上に置く。
    modelOffset_ = {0.0f, 7.0f, 0.0f};
    baseModel_ = std::make_unique<Model>();
    baseModel_->Initialize("Cube");
    baseModel_->SetEnvironmentTexture("skybox.dds");
    baseModel_->SetScale({3.0f, 1.0f, 3.0f});
    baseCollider_ = std::make_unique<Collision::Collider>();
    baseCollider_->SetName("MainTowerBase")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(static_cast<Tower*>(this))
        ->AddAttribute(CollisionAttribute::Tower)
        ->AddIgnore(CollisionAttribute::Tower)
        ->SetSize(Vector3{6.0f, 2.0f, 6.0f})
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
    baseCollider_->SetTranslate(center + GetColliderOffset());
}

void MainTower::Draw() {
    baseModel_->Draw();
    Tower::Draw();
}

void MainTower::SetHovered(bool _hovered) {
    // 色は ApplyModelColor() が選択状態と被弾フラッシュの両方から決める。
    // ここで直接色を塗ると、被弾中にホバーが切り替わったときフラッシュが消えてしまう
    hovered_ = _hovered;
    ApplyModelColor();
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

    // 柱と土台の両方を光らせる。面積が大きいほど視界の端でも気付きやすい
    if (model_) {
        model_->SetColor(LerpColor(
            hovered_ ? HOVERED_COLOR : PILLAR_NORMAL_COLOR, damageFlashColor_, flash));
    }
    if (baseModel_) {
        baseModel_->SetColor(LerpColor(
            hovered_ ? HOVERED_COLOR : BASE_NORMAL_COLOR, damageFlashColor_, flash));
    }
}
