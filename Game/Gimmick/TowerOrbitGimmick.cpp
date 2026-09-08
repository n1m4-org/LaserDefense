#include "TowerOrbitGimmick.hpp"

void TowerOrbitGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
}

void TowerOrbitGimmick::Update(float _deltaTime) {
    (void)_deltaTime;
    // 指定タワーの周回判定は担当側で実装する。
}

void TowerOrbitGimmick::Draw() const {
    // AoEの描画は担当側で実装する。
}
