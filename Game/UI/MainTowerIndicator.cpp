#define NOMINMAX
#include "MainTowerIndicator.hpp"

#include <algorithm>
#include <cmath>

#include "Camera/Controller/CameraController.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Tower/MainTower.hpp"

namespace {
    constexpr float EDGE_PADDING = 12.0f;
    constexpr float EPSILON = 0.0001f;
}

void MainTowerIndicator::Initialize() {
    LoadConfig();
    arrow_.Initialize("arrow.png");
    arrow_.SetSize(arrowSize_);
    arrow_.SetAnchorPoint({0.5f, 0.5f});
    arrow_.SetColor({0.25f, 0.95f, 0.55f, 1.0f});
    visible_ = false;
}

void MainTowerIndicator::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("MainTowerIndicator", "MainTowerIndicator")) return;
    const auto groups = json->GetGroups("MainTowerIndicator");
    const auto display = groups.find("Display");
    if (display == groups.end()) return;
    const auto size = display->second.find("Size");
    if (size == display->second.end()) return;
    if (const auto value = std::get_if<Vector2>(&size->second)) {
        arrowSize_.x = std::max(std::abs(value->x), 1.0f);
        arrowSize_.y = std::max(std::abs(value->y), 1.0f);
    }
}

void MainTowerIndicator::Update(const MainTower* _target) {
    visible_ = false;
    if (!_target || !_target->IsActive() || !_target->IsDefenseTarget()) return;

    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    const auto screen = Singleton<Screen>::GetInstance();
    const float width = screen->Width();
    const float height = screen->Height();
    const float screenMargin = std::max(arrowSize_.x, arrowSize_.y) * 0.5f + EDGE_PADDING;
    if (!camera || width <= screenMargin * 2.0f || height <= screenMargin * 2.0f) return;

    // 斜め見下ろしでは高さがあるほど画面上の方向がずれるため、足元を案内する。
    const Vector3 worldPosition = _target->GetPosition();
    const Vector4 clip = MathUtils::Matrix::Transform(
        Vector4{worldPosition.x, worldPosition.y, worldPosition.z, 1.0f},
        camera->GetViewProjection());
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y)
        || !std::isfinite(clip.z) || !std::isfinite(clip.w)
        || std::abs(clip.w) <= EPSILON) return;

    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float ndcZ = clip.z / clip.w;
    const bool inFront = clip.w > 0.0f;
    const bool onScreen = inFront && ndcZ >= 0.0f && ndcZ <= 1.0f
        && std::abs(ndcX) <= 1.0f && std::abs(ndcY) <= 1.0f;
    if (onScreen) return;

    // 背後にある場合は投影方向を反転し、実際に振り向くべき側へ矢印を出す。
    Vector2 direction{ndcX, -ndcY};
    if (!inFront) direction = direction * -1.0f;
    const float length = std::hypot(direction.x, direction.y);
    if (length <= EPSILON) direction = {0.0f, -1.0f};
    else direction = direction * (1.0f / length);

    const Vector2 center{width * 0.5f, height * 0.5f};
    const Vector2 halfArea{center.x - screenMargin, center.y - screenMargin};
    const float scaleX = std::abs(direction.x) > EPSILON
        ? halfArea.x / std::abs(direction.x) : INFINITY;
    const float scaleY = std::abs(direction.y) > EPSILON
        ? halfArea.y / std::abs(direction.y) : INFINITY;
    const Vector2 position = center + direction * std::min(scaleX, scaleY);

    arrow_.SetPosition(position);
    // arrow.png は左向きなので、左ベクトルから目的方向まで回転させる。
    arrow_.SetRotation(std::atan2(direction.y, direction.x) - MathUtils::F_PI);
    arrow_.Update();
    visible_ = true;
}

void MainTowerIndicator::Draw() {
    if (!visible_) return;
    arrow_.Draw();
}
