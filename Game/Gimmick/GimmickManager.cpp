#include "GimmickManager.hpp"

#include <variant>

#include "Gimmick/RouteGimmick.hpp"
#include "Gimmick/TowerDefenseGimmick.hpp"
#include "Gimmick/TowerOrbitGimmick.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"

#ifdef _DEBUG
#include "imgui.h"
#endif

#undef min
#undef max

void GimmickManager::Initialize(const GimmickContext& _context) {
    context_ = _context;
    activeGimmick_.reset();
    spawnTime_ = 0.0f;
    LoadConfig();
    RouteGimmick::ResetTutorialProgress();
}

void GimmickManager::Update(float _deltaTime) {
    if (pendingStart_) {
        const GimmickType type = *pendingStart_;
        pendingStart_.reset();
        StartGimmick(type);
    }

    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f) return;
    if (debugPaused_) return;

    if (activeGimmick_) {
        activeGimmick_->Update(_deltaTime);
        if (activeGimmick_->IsFinished()) {
            activeGimmick_.reset();
            spawnTime_ = 0.0f;
        }
        return;
    }

    spawnTime_ += _deltaTime;
    if (spawnTime_ < spawnInterval_) return;
    spawnTime_ = 0.0f;
    StartRandomGimmick();
}

void GimmickManager::Draw() const {
    if (activeGimmick_) activeGimmick_->Draw();
}

void GimmickManager::Debug() {
#ifdef _DEBUG
    ImGui::Begin("GimmickManager");
    ImGui::Text("Active: %s", activeGimmick_ ? "Yes" : "No");

    if (ImGui::Button("Route")) pendingStart_ = GimmickType::Route;
    ImGui::SameLine();
    if (ImGui::Button("TowerDefense")) pendingStart_ = GimmickType::TowerDefense;
    ImGui::SameLine();
    if (ImGui::Button("TowerOrbit")) pendingStart_ = GimmickType::TowerOrbit;

    if (activeGimmick_) {
        ImGui::Checkbox("Paused", &debugPaused_);
        ImGui::SameLine();
        if (ImGui::Button("Restart")) pendingStart_ = activeGimmick_->GetType();
    }
    ImGui::End();

    if (activeGimmick_) activeGimmick_->Debug();
#endif
}

void GimmickManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Gimmick")) return;
    const auto groups = json->GetGroups("Gimmick");

    const auto read = [](const auto& _group, const char* _key, float _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto floatValue = std::get_if<float>(&entry->second)) return *floatValue;
        if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*integerValue);
        }
        return _fallback;
    };

    if (const auto spawn = groups.find("Spawn"); spawn != groups.end()) {
        spawnInterval_ = read(spawn->second, "IntervalSeconds", spawnInterval_);
    }
    if (const auto lottery = groups.find("Lottery"); lottery != groups.end()) {
        routeWeight_ = read(lottery->second, "RouteWeight", routeWeight_);
        towerDefenseWeight_ = read(
            lottery->second, "TowerDefenseWeight", towerDefenseWeight_);
        towerOrbitWeight_ = read(lottery->second, "TowerOrbitWeight", towerOrbitWeight_);
    }

    spawnInterval_ = std::isfinite(spawnInterval_) ? std::max(spawnInterval_, 0.0f) : 30.0f;
    routeWeight_ = std::isfinite(routeWeight_) ? std::max(routeWeight_, 0.0f) : 1.0f;
    towerDefenseWeight_ = std::isfinite(towerDefenseWeight_)
        ? std::max(towerDefenseWeight_, 0.0f) : 1.0f;
    towerOrbitWeight_ = std::isfinite(towerOrbitWeight_)
        ? std::max(towerOrbitWeight_, 0.0f) : 1.0f;
}

void GimmickManager::StartRandomGimmick() {
    const float totalWeight = routeWeight_ + towerDefenseWeight_ + towerOrbitWeight_;
    if (totalWeight <= 0.0f) return;

    const float lottery = MathUtils::Random(0.0f, totalWeight);
    GimmickType type = GimmickType::TowerOrbit;
    if (lottery < routeWeight_) {
        type = GimmickType::Route;
    } else if (lottery < routeWeight_ + towerDefenseWeight_) {
        type = GimmickType::TowerDefense;
    }

    StartGimmick(type);
}

void GimmickManager::StartGimmick(GimmickType _type) {
    activeGimmick_ = CreateGimmick(_type);
    if (activeGimmick_) activeGimmick_->Initialize(context_);
    spawnTime_ = 0.0f;
}

std::unique_ptr<IGimmick> GimmickManager::CreateGimmick(GimmickType _type) const {
    switch (_type) {
    case GimmickType::Route:
        return std::make_unique<RouteGimmick>();
    case GimmickType::TowerDefense:
        return std::make_unique<TowerDefenseGimmick>();
    case GimmickType::TowerOrbit:
        return std::make_unique<TowerOrbitGimmick>();
    }
    return nullptr;
}
