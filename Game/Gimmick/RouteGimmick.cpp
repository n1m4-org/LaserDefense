#include "RouteGimmick.hpp"

void RouteGimmick::Initialize(const GimmickContext& _context) {
    context_ = _context;
    state_ = GimmickState::Active;
}

void RouteGimmick::Update(float _deltaTime) {
    (void)_deltaTime;
    // 円を指定順に巡る処理は担当側で実装する。
}

void RouteGimmick::Draw() const {
    // AoEの描画は担当側で実装する。
}
