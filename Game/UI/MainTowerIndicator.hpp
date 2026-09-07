#ifndef MAIN_TOWER_INDICATOR_HPP_
#define MAIN_TOWER_INDICATOR_HPP_

#include "Sprite.hpp"

class MainTower;

/// 画面外にある防衛対象の方向を、画面端の矢印で示す。
class MainTowerIndicator final {
    Sprite arrow_{};
    Vector2 arrowSize_{28.0f, 28.0f};
    bool visible_ = false;

public:
    void Initialize();
    void Update(const MainTower* _target);
    void Draw();

private:
    void LoadConfig();
};

#endif // MAIN_TOWER_INDICATOR_HPP_
