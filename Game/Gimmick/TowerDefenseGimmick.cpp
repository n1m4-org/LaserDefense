#include "TowerDefenseGimmick.hpp"

void TowerDefenseGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
}

void TowerDefenseGimmick::Update(float _deltaTime) {
    (void)_deltaTime;
    // 指定タワー周辺の撃破数判定は担当側で実装する。
}

void TowerDefenseGimmick::Draw() const {
    // AoEの描画は担当側で実装する。
}
