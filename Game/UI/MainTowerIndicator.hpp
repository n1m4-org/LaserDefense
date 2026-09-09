#ifndef MAIN_TOWER_INDICATOR_HPP_
#define MAIN_TOWER_INDICATOR_HPP_

#include "Math/Vector3.hpp"
#include "Math/Vector4.hpp"
#include "Sprite.hpp"

class MainTower;

/// 画面外にあるワールド座標の方向を、画面端の矢印で示す。
class MainTowerIndicator final {
    Sprite arrow_{};
    Vector2 arrowSize_{28.0f, 28.0f};
    Vector4 color_{0.25f, 0.95f, 0.55f, 1.0f};
    bool visible_ = false;
    float opacity_ = 1.0f;

public:
    void Initialize();
    void Update(const MainTower* _target);
    void Update(const Vector3* _worldPosition);
    void Draw();
    void SetOpacity(float _opacity);
    void SetColor(const Vector4& _color);

private:
    void LoadConfig();
};

#endif // MAIN_TOWER_INDICATOR_HPP_
