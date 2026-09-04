#define NOMINMAX

#include "EnemyManager.hpp"

#include <algorithm>
#include <cmath>

#include "Enemy.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Random/RandomEngine.hpp"
#include "Score/ScoreManager.hpp"
#include "Combo/ComboManager.hpp"
#include "Tower/MainTower.hpp"

EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize() {
    LoadConfig();
    Model::Load(modelName_);
    spawnTimer_.SetDuration(std::chrono::milliseconds{
        static_cast<int64_t>(spawnIntervalSeconds_ * 1000.0f)});
    spawnTimer_.Start();
}

void EnemyManager::SetTargetPosition(float _x, float _z) {
    targetPosition_ = {_x, 0.0f, _z};
    for (const auto& enemy : enemies_) {
        enemy->SetMovement(targetPosition_, moveSpeed_);
    }
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

    if (const auto movement = groups.find("Movement"); movement != groups.end()) {
        moveSpeed_ = read(movement->second, "Speed", moveSpeed_);
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
        spawnIntervalSeconds_ = read(spawn->second, "IntervalSeconds", spawnIntervalSeconds_);
        spawnCount_ = read(spawn->second, "Count", spawnCount_);
        spawnRange_ = read(spawn->second, "Range", spawnRange_);
        spawnExcludeRange_ = read(spawn->second, "ExcludeRange", spawnExcludeRange_);
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

    if (spawnIntervalSeconds_ <= 0.0f) {
        spawnIntervalSeconds_ = 2.0f;
    }
    if (moveSpeed_ < 0.0f) {
        moveSpeed_ = 0.0f;
    }
    spawnCount_ = std::clamp(spawnCount_, 0, 128);
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
    enemies_.push_back(std::move(enemy));
}

void EnemyManager::Update(float _deltaTime) {
    if (spawnTimer_.Check()) {
        const auto random = Singleton<RandomEngine>::GetInstance();
        const float halfWidth = spawnRange_.x * 0.5f;
        const float halfDepth = spawnRange_.y * 0.5f;
        // 目標のメインタワーを中心とした矩形内には出現させない。
        // 設定で全域が除外されても無限ループしないよう上限を設ける。
        int32_t spawned = 0;
        for (int attempt = 0; attempt < 128 * spawnCount_ && spawned < spawnCount_; ++attempt) {
            const Vector3 position{random->Get(-halfWidth, halfWidth), 0.0f,
                                   random->Get(-halfDepth, halfDepth)};
            const bool excluded = spawnExcludeRange_.x > 0.0f && spawnExcludeRange_.y > 0.0f
                && std::abs(position.x - targetPosition_.x) <= spawnExcludeRange_.x * 0.5f
                && std::abs(position.z - targetPosition_.z) <= spawnExcludeRange_.y * 0.5f;
            if (!excluded) {
                SpawnEnemy(position);
                ++spawned;
            }
        }
        spawnTimer_.Restart();
    }

    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Update(_deltaTime);
        }
    }

    // 削除の前に報酬を回収する（死亡演出が1フレームで終わる設定でも取りこぼさない）
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
            if (mainTower_) {
                mainTower_->TakeDamage(enemy->GetTowerDamage());
            }
        }

        // ConsumeDefeatReward() は1体につき1回だけ true を返すので二重加算されない
        if (!enemy->ConsumeDefeatReward()) {
            continue;
        }

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
