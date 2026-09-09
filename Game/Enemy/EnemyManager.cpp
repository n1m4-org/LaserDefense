#define NOMINMAX

#include "EnemyManager.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "Enemy.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Random/RandomEngine.hpp"
#include "Score/ScoreManager.hpp"
#include "Combo/ComboManager.hpp"
#include "Tower/TowerManager.hpp"
#include "Math/MathUtils.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

namespace {
    constexpr const char* HIT_EFFECT_TEMPLATE = "EnemyHitEffect";
    constexpr const char* HIT_EFFECT_SPAWN = "EnemyHitEffectSpawn";
    constexpr const char* DEATH_EFFECT_SPAWN = "EnemyDeathEffectSpawn";
    constexpr const char* DEATH_EFFECT_TEMPLATES[] = {
        "EnemyDeathEffectSmall",
        "EnemyDeathEffectMedium",
        "EnemyDeathEffectLarge"
    };
    constexpr float DEATH_EFFECT_SIZES[] = {0.8f, 0.9f, 1.0f};
}

EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize(GESTD::ReferencePtr<ParticleSystem> _particleSystem) {
    particleSystem_ = _particleSystem;
    LoadConfig();
    elapsedSeconds_ = 0.0f;
    spawnElapsedSeconds_ = 0.0f;
    recentDefeatPositions_.clear();
    Model::Load(modelName_);
    InitializeHitEffect();
    InitializeDeathEffect();
}

void EnemyManager::InitializeHitEffect() {
    if (!particleSystem_) return;

    particleSystem_->RegisterSpawnFunc(HIT_EFFECT_SPAWN,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            // 方位角と上向き成分から、床側を含まない上半球方向を作る。
            const float y = MathUtils::Random(0.15f, 1.0f);
            const float angle = MathUtils::Random(0.0f, MathUtils::F_PI * 2.0f);
            const float horizontal = std::sqrt(std::max(1.0f - y * y, 0.0f));
            const Vector3 direction{
                std::cos(angle) * horizontal,
                y,
                std::sin(angle) * horizontal};
            _position = _center + direction * MathUtils::Random(0.0f, 0.15f);
            _velocity = direction * MathUtils::Random(6.0f, 14.0f);
        });

    const auto makeEmitter = [](const Vector4& _startColor, const Vector4& _endColor) {
        ParticleSystem::EmitterConfig emitter;
        emitter.texture = "white_x16.png";
        emitter.frequency = 0.0f;
        emitter.duration = 0.0f;
        emitter.spawnCount = 5;
        emitter.size = {0.44f, 0.44f, 0.44f};
        emitter.particleLifetime = 0.8f;
        emitter.spawnFuncKey = HIT_EFFECT_SPAWN;
        emitter.colorKeys = {
            GradientKey<Vector4>{0.0f, _startColor},
            GradientKey<Vector4>{1.0f, _endColor}
        };
        emitter.sizeKeys = {
            GradientKey<Vector3>{0.0f, {0.44f, 0.44f, 0.44f}},
            GradientKey<Vector3>{1.0f, {0.04f, 0.04f, 0.04f}}
        };
        return emitter;
    };

    ParticleSystem::Template hitEffect;
    hitEffect.emitters.push_back(makeEmitter(
        {1.0f, 0.05f, 0.02f, 1.0f}, {0.5f, 0.0f, 0.0f, 0.0f}));
    hitEffect.emitters.push_back(makeEmitter(
        {1.0f, 0.45f, 0.02f, 1.0f}, {1.0f, 0.1f, 0.0f, 0.0f}));
    particleSystem_->Register(HIT_EFFECT_TEMPLATE, hitEffect, true);
}

void EnemyManager::InitializeDeathEffect() {
    if (!particleSystem_) return;

    particleSystem_->RegisterSpawnFunc(DEATH_EFFECT_SPAWN,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            // 同じ場所から機械的に並ばないよう、敵の中心付近で少しだけ散らす。
            _position = _center + Vector3{
                MathUtils::Random(-0.15f, 0.15f),
                MathUtils::Random(-0.05f, 0.15f),
                MathUtils::Random(-0.15f, 0.15f)};
            _velocity = {
                MathUtils::Random(-0.4f, 0.4f),
                MathUtils::Random(5.0f, 7.0f),
                MathUtils::Random(-0.4f, 0.4f)};
        });

    for (std::size_t i = 0; i < std::size(DEATH_EFFECT_TEMPLATES); ++i) {
        const float size = DEATH_EFFECT_SIZES[i];
        ParticleSystem::EmitterConfig emitter;
        emitter.texture = "white_x16.png";
        emitter.frequency = 0.3f;
        emitter.duration = 1.5f;
        emitter.spawnCount = 1;
        emitter.size = {size, size, size};
        emitter.particleLifetime = 0.9f;
        emitter.spawnFuncKey = DEATH_EFFECT_SPAWN;
        emitter.colorKeys = {
            GradientKey<Vector4>{0.0f, {1.0f, 0.72f, 0.08f, 1.0f}},
            GradientKey<Vector4>{1.0f, {1.0f, 0.3f, 0.0f, 0.0f}}
        };
        emitter.sizeKeys = {
            GradientKey<Vector3>{0.0f, {size, size, size}},
            GradientKey<Vector3>{1.0f, {0.04f, 0.04f, 0.04f}}
        };

        ParticleSystem::Template deathEffect;
        deathEffect.emitters.push_back(emitter);
        particleSystem_->Register(DEATH_EFFECT_TEMPLATES[i], deathEffect, true);
    }
}

void EnemyManager::ApplyShockwave(const Vector3& _center, float _radius, float _speed) {
    for (const auto& enemy : enemies_) {
        enemy->ApplyShockwave(_center, _radius, _speed);
    }
}

void EnemyManager::SetTargetPosition(float _x, float _z) {
    targetPosition_ = {_x, 0.0f, _z};
    for (const auto& enemy : enemies_) {
        enemy->SetMovement(targetPosition_, moveSpeed_);
    }
}

void EnemyManager::SetSpawnExclusionPositions(const std::vector<Vector3>& _positions) {
    spawnExclusionPositions_ = _positions;
}

void EnemyManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Enemy", "Enemy")) {
        return;
    }

    const auto groups = json->GetGroups("Enemy");
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

    if (const auto appearance = groups.find("Appearance"); appearance != groups.end()) {
        modelName_ = read(appearance->second, "Model", modelName_);
        modelColor_ = read(appearance->second, "Color", modelColor_);
        modelScale_ = read(appearance->second, "Scale", modelScale_);
        modelOffset_ = read(appearance->second, "ModelOffset", modelOffset_);
    }

    if (const auto health = groups.find("Health"); health != groups.end()) {
        maxHp_ = read(health->second, "MaxHp", maxHp_);
        knockbackBrake_ = read(health->second, "KnockbackBrake", knockbackBrake_);
    }

    if (const auto animation = groups.find("SpawnAnimation"); animation != groups.end()) {
        spawnAnimationDuration_ = read(animation->second, "DurationSeconds", spawnAnimationDuration_);
        spawnStartScale_ = read(animation->second, "StartScale", spawnStartScale_);
        spawnRotations_ = read(animation->second, "Rotations", spawnRotations_);
        const int32_t moveDuringAnimation = read(
            animation->second, "MoveDuringAnimation",
            static_cast<int32_t>(moveDuringSpawnAnimation_));
        moveDuringSpawnAnimation_ = moveDuringAnimation != 0;
    }

    if (const auto animation = groups.find("DeathAnimation"); animation != groups.end()) {
        deathAnimationDuration_ = read(animation->second, "DurationSeconds", deathAnimationDuration_);
        deathPeakScale_ = read(animation->second, "PeakScale", deathPeakScale_);
        deathEndScale_ = read(animation->second, "EndScale", deathEndScale_);
        deathExpandRatio_ = read(animation->second, "ExpandRatio", deathExpandRatio_);
    }

    if (const auto spawn = groups.find("Spawn"); spawn != groups.end()) {
        spawnRange_ = read(spawn->second, "Range", spawnRange_);
        spawnExcludeRange_ = read(spawn->second, "ExcludeRange", spawnExcludeRange_);
        maxEnemyCount_ = read(spawn->second, "MaxEnemyCount", maxEnemyCount_);
        spawnIntervalSeconds_ = read(
            spawn->second, "IntervalSeconds", spawnIntervalSeconds_);
        initialSpawnCount_ = read(spawn->second, "InitialCount", initialSpawnCount_);
        spawnCountIncreaseIntervalSeconds_ = read(
            spawn->second, "CountIncreaseIntervalSeconds",
            spawnCountIncreaseIntervalSeconds_);
    }

    if (const auto movement = groups.find("Movement"); movement != groups.end()) {
        moveSpeed_ = read(movement->second, "Speed", moveSpeed_);
    }

    // 敵1体あたりの獲得スコア。ここの値を変えるだけで得点バランスを調整できる
    if (const auto score = groups.find("Score"); score != groups.end()) {
        scoreValue_ = read(score->second, "Value", scoreValue_);
        const int32_t awardOnTowerHit = read(
            score->second, "AwardOnTowerHit",
            static_cast<int32_t>(awardRewardOnTowerHit_));
        awardRewardOnTowerHit_ = awardOnTowerHit != 0;
    }


    // 敵1体がタワーへ到達したときのダメージ。ここの値を変えるだけで耐久バランスを調整できる
    if (const auto towerDamage = groups.find("TowerDamage"); towerDamage != groups.end()) {
        towerDamage_ = read(towerDamage->second, "Value", towerDamage_);
    }

    maxEnemyCount_ = std::clamp(maxEnemyCount_, 0, 10000);
    spawnIntervalSeconds_ = std::isfinite(spawnIntervalSeconds_)
        ? std::max(spawnIntervalSeconds_, 0.01f) : 1.0f;
    initialSpawnCount_ = std::clamp(initialSpawnCount_, 0, 128);
    spawnCountIncreaseIntervalSeconds_ =
        std::isfinite(spawnCountIncreaseIntervalSeconds_)
        ? std::max(spawnCountIncreaseIntervalSeconds_, 0.01f) : 30.0f;
    moveSpeed_ = std::isfinite(moveSpeed_) ? std::max(moveSpeed_, 0.0f) : 2.0f;
    spawnAnimationDuration_ = std::max(spawnAnimationDuration_, 0.0f);
    spawnStartScale_ = std::max(spawnStartScale_, 0.0001f);
    deathAnimationDuration_ = std::max(deathAnimationDuration_, 0.0f);
    deathPeakScale_ = std::max(deathPeakScale_, 0.0001f);
    deathEndScale_ = std::max(deathEndScale_, 0.0001f);
    deathExpandRatio_ = std::clamp(deathExpandRatio_, 0.01f, 0.99f);
    spawnRange_.x = std::abs(spawnRange_.x);
    spawnRange_.y = std::abs(spawnRange_.y);
    spawnExcludeRange_.x = std::isfinite(spawnExcludeRange_.x) ? std::abs(spawnExcludeRange_.x) : 30.0f;
    spawnExcludeRange_.y = std::isfinite(spawnExcludeRange_.y) ? std::abs(spawnExcludeRange_.y) : 30.0f;
    scoreValue_ = std::max(scoreValue_, 0);
    towerDamage_ = std::max(towerDamage_, 0.0f);
}

void EnemyManager::SpawnEnemy(const Vector3& _position) {
    auto enemy = std::make_unique<Enemy>();
    enemy->SetParticleSystem(particleSystem_);
    enemy->SetHealth(maxHp_, knockbackBrake_);
    enemy->SetAppearance(modelName_, modelScale_, modelOffset_, modelColor_);
    enemy->SetMovement(targetPosition_, moveSpeed_);
    enemy->SetSpawnAnimation(spawnAnimationDuration_, spawnStartScale_,
                             spawnRotations_, moveDuringSpawnAnimation_);
    enemy->SetDeathAnimation(deathAnimationDuration_, deathPeakScale_,
                             deathEndScale_, deathExpandRatio_);
    enemy->SetDefeatReward(scoreValue_, awardRewardOnTowerHit_);
    enemy->SetTowerDamage(towerDamage_);
    enemy->Initialize();
    enemy->SetPosition(_position);
    enemy->Update(0.0f);
    enemies_.push_back(std::move(enemy));
}

void EnemyManager::SpawnExtraEnemy(const Vector3& _position) {
    if (maxEnemyCount_ <= 0
        || enemies_.size() >= static_cast<std::size_t>(maxEnemyCount_)) return;
    SpawnEnemy(_position);
}

void EnemyManager::SpawnWave() {
    if (maxEnemyCount_ <= 0
        || enemies_.size() >= static_cast<std::size_t>(maxEnemyCount_)) return;

    const auto random = Singleton<RandomEngine>::GetInstance();
    const float halfWidth = spawnRange_.x * 0.5f;
    const float halfDepth = spawnRange_.y * 0.5f;
    const int32_t available = maxEnemyCount_ - static_cast<int32_t>(enemies_.size());
    const float countIncrease = std::floor(
        elapsedSeconds_ / spawnCountIncreaseIntervalSeconds_);
    const int32_t requestedCount = initialSpawnCount_ + static_cast<int32_t>(
        std::min(countIncrease, static_cast<float>(maxEnemyCount_)));
    const int32_t spawnCount = std::min(requestedCount, available);

    // マップ全域へ出し、各タワーを中心とする除外矩形は空ける。
    int32_t spawned = 0;
    for (int attempt = 0; attempt < 128 * spawnCount && spawned < spawnCount; ++attempt) {
        const Vector3 position{
            random->Get(-halfWidth, halfWidth),
            0.0f,
            random->Get(-halfDepth, halfDepth)};
        const bool excluded = spawnExcludeRange_.x > 0.0f && spawnExcludeRange_.y > 0.0f
            && std::any_of(spawnExclusionPositions_.begin(), spawnExclusionPositions_.end(),
                [&](const Vector3& _towerPosition) {
                    return std::abs(position.x - _towerPosition.x) <= spawnExcludeRange_.x * 0.5f
                        && std::abs(position.z - _towerPosition.z) <= spawnExcludeRange_.y * 0.5f;
                });
        if (!excluded) {
            SpawnEnemy(position);
            ++spawned;
        }
    }
}

void EnemyManager::Update(float _deltaTime) {
    // リザルト中は gameDelta=0 が渡るため、経過時間とスポーン時間は進まない。
    if (std::isfinite(_deltaTime) && _deltaTime > 0.0f) {
        elapsedSeconds_ += _deltaTime;

        if (!spawnSuspended_) {
            spawnElapsedSeconds_ += _deltaTime;
            if (spawnElapsedSeconds_ >= spawnIntervalSeconds_) {
                spawnElapsedSeconds_ = std::fmod(spawnElapsedSeconds_, spawnIntervalSeconds_);
                SpawnWave();
            }
        }
    }

    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Update(_deltaTime);
        }
    }

    // 削除の前に報酬を回収する（死亡演出が1フレームで終わる設定でも取りこぼさない）
    recentDefeatPositions_.clear();
    CollectDefeatRewards();

    std::erase_if(enemies_, [](const std::unique_ptr<Enemy>& _enemy) {
        return _enemy->IsDeathAnimationFinished();
    });
}

void EnemyManager::CollectDefeatRewards() {
    for (const auto& enemy : enemies_) {
        // 取り逃がし（タワーへ到達された）はコンボを途切れさせる。
        // 撃破報酬の判定より先に行い、同フレームの撃破が新しいコンボとして始まるようにする
        if (enemy->ConsumeTowerReach()) {
            if (comboManager_ && comboManager_->IsBreakOnTowerReach()) {
                comboManager_->Break();
            }
            // 到達を許した1体につき1回だけタワーの HP を削る。
            // 1体あたりのダメージは Enemy が持っているので、敵の種類ごとに変えられる
            if (towerManager_) {
                towerManager_->TakeDamage(enemy->GetTowerDamage());
            }
        }

        // ConsumeDefeatReward() は1体につき1回だけ true を返すので二重加算されない
        if (!enemy->ConsumeDefeatReward()) {
            continue;
        }

        recentDefeatPositions_.push_back(enemy->GetPosition());

        // 先にコンボを進めてから倍率を取る。こうすると倒したその1体にも倍率が乗る
        int32_t multiplier = 1;
        if (comboManager_) {
            comboManager_->AddCombo();
            multiplier = comboManager_->GetMultiplier();
        }

        if (scoreManager_) {
            // ScoreManager 側の倍率（アイテム効果など）とは掛け合わせになる。
            // 第2引数はコンボ倍率で、加算演出をどれだけ派手にするかにだけ使われる
            scoreManager_->AddScore(enemy->GetScoreValue() * multiplier, multiplier);
        }
    }
}

void EnemyManager::Draw() const {
    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Draw();
        }
    }
}
