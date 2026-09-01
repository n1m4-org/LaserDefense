#include "EnemyManager.hpp"

#include <algorithm>
#include <cmath>

#include "Enemy.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Random/RandomEngine.hpp"

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

    if (const auto spawn = groups.find("Spawn"); spawn != groups.end()) {
        spawnIntervalSeconds_ = read(spawn->second, "IntervalSeconds", spawnIntervalSeconds_);
        spawnRange_ = read(spawn->second, "Range", spawnRange_);
    }

    if (spawnIntervalSeconds_ <= 0.0f) {
        spawnIntervalSeconds_ = 3.0f;
    }
    if (moveSpeed_ < 0.0f) {
        moveSpeed_ = 0.0f;
    }
    spawnRange_.x = std::abs(spawnRange_.x);
    spawnRange_.y = std::abs(spawnRange_.y);
}

void EnemyManager::SpawnEnemy(const Vector3& _position) {
    auto enemy = std::make_unique<Enemy>();
    enemy->SetAppearance(modelName_, modelScale_, modelOffset_, modelColor_);
    enemy->SetMovement(targetPosition_, moveSpeed_);
    enemy->Initialize();
    enemy->SetPosition(_position);
    enemies_.push_back(std::move(enemy));
}

void EnemyManager::Update(float _deltaTime) {
    if (spawnTimer_.Check()) {
        const auto random = Singleton<RandomEngine>::GetInstance();
        const float halfWidth = spawnRange_.x * 0.5f;
        const float halfDepth = spawnRange_.y * 0.5f;
        SpawnEnemy({random->Get(-halfWidth, halfWidth), 0.0f,
                    random->Get(-halfDepth, halfDepth)});
        spawnTimer_.Restart();
    }

    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Update(_deltaTime);
        }
    }

    std::erase_if(enemies_, [](const std::unique_ptr<Enemy>& _enemy) {
        return !_enemy->IsAlive();
    });
}

void EnemyManager::Draw() const {
    for (const auto& enemy : enemies_) {
        if (enemy->IsActive()) {
            enemy->Draw();
        }
    }
}
