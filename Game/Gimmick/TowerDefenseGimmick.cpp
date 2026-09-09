#include "TowerDefenseGimmick.hpp"

#include <algorithm>
#include <cstdint>
#include <variant>

#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

void TowerDefenseGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
    LoadConfig();
}

void TowerDefenseGimmick::Update(float _deltaTime) {
    (void)_deltaTime;
    // 指定タワー周辺の撃破数判定は担当側で実装する。
}

void TowerDefenseGimmick::Draw() const {
    // AoEの描画は担当側で実装する。
}

void TowerDefenseGimmick::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Gimmick")) return;
    const auto groups = json->GetGroups("Gimmick");
    const auto defense = groups.find("TowerDefense");
    if (defense == groups.end()) return;
    const auto entry = defense->second.find("TimeLimitSeconds");
    if (entry == defense->second.end()) return;
    if (const auto value = std::get_if<float>(&entry->second)) {
        timeLimitSeconds_ = std::max(*value, 0.01f);
    } else if (const auto valueInt = std::get_if<int32_t>(&entry->second)) {
        timeLimitSeconds_ = std::max(static_cast<float>(*valueInt), 0.01f);
    }
}
