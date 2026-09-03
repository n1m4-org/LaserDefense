#define NOMINMAX
#include "PlayerCamera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

#include "Camera/Controller/CameraController.hpp"
#include "GameObject/Player/Player.h"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"

void PlayerCamera::Initialize(const Player& _player) {
    LoadConfig();
    focus_ = _player.GetPosition() + _player.GetModelOffset();
    ApplyCamera();
}

void PlayerCamera::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("PlayerCamera", "PlayerCamera")) return;
    const auto groups = json->GetGroups("PlayerCamera");
    const auto follow = groups.find("Follow");
    if (follow == groups.end()) return;
    const auto read = [&](const char* _key, float _fallback) {
        const auto entry = follow->second.find(_key);
        if (entry == follow->second.end()) return _fallback;
        float value = _fallback;
        if (const auto number = std::get_if<float>(&entry->second)) value = *number;
        else if (const auto integer = std::get_if<int32_t>(&entry->second)) value = static_cast<float>(*integer);
        return std::isfinite(value) ? value : _fallback;
    };
    followSpeed_ = std::max(read("FollowSpeed", followSpeed_), 0.0f);
    distance_ = std::max(read("Distance", distance_), 1.0f);
}

void PlayerCamera::Update(const Player& _player, float _deltaTime) {
    if (std::isfinite(_deltaTime) && _deltaTime > 0.0f) {
        // フレームレートによらず滑らかに追従。速度による距離変更は行わない。
        const float blend = 1.0f - std::exp(-followSpeed_ * std::min(_deltaTime, 0.1f));
        focus_ += (_player.GetPosition() + _player.GetModelOffset() - focus_) * blend;
    }
    ApplyCamera();
}

void PlayerCamera::ApplyCamera() const {
    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    if (!camera) return;
    // 既存シーンの斜め見下ろし角度を維持する。
    const float pitch = std::atan2(65.0f, 45.0f);
    camera->transform_.translate = focus_
        + Vector3{0.0f, std::sin(pitch) * distance_, -std::cos(pitch) * distance_};
    camera->transform_.rotate = Vector3{pitch, 0.0f, 0.0f};
    camera->SetFov(1.0f);
    camera->SetFar(std::max(200.0f, distance_ + 100.0f));
    camera->Update();
}
