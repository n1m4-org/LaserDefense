#include "ParticleTest.h"

#include "Camera/Controller/CameraController.hpp"
#include "Input.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Time/Time.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

namespace {
    constexpr const char* kLaserTemplateName = "LaserBeam";
    constexpr const char* kLaserSpawnFuncKey = "LaserBeamSpawn";
    constexpr float kBeamHalfLength = 3.0f;
    constexpr float kBeamHalfWidth = 0.15f;
    constexpr float kMoveSpeed = 5.0f;
}

void ParticleTest::Initialize() {
    if (const auto camera = Singleton<CameraController>::GetInstance()->GetActive()) {
        camera->transform_.translate = {0.f, 20.f, 0.f};
        camera->transform_.rotate = Vector3{MathUtils::F_PI * 0.5f, 0.f, 0.f};
    }

    RegisterLaserTemplate();
}

void ParticleTest::RegisterLaserTemplate() {
    Particle()->RegisterSpawnFunc(kLaserSpawnFuncKey, [](const Vector3& _center, Vector3& _outPos, Vector3& _outVel) {
        _outPos = _center + Vector3{
            MathUtils::Random(-kBeamHalfLength, kBeamHalfLength),
            0.f,
            MathUtils::Random(-kBeamHalfWidth, kBeamHalfWidth)
        };
        _outVel = Vector3{
            MathUtils::Random(-0.3f, 0.3f),
            MathUtils::Random(1.5f, 3.0f),
            MathUtils::Random(-0.3f, 0.3f)
        };
    });

    ParticleSystem::EmitterConfig config;
    config.texture = "white_x16.png";
    config.frequency = 0.02f;
    config.duration = 300.f;
    config.spawnCount = 2;
    config.size = {0.15f, 0.15f, 0.15f};
    config.particleLifetime = 0.8f;
    config.spawnFuncKey = kLaserSpawnFuncKey;
    config.colorKeys = {
        GradientKey<Vector4>{0.f,  {1.f, 1.f, 1.f, 1.f}},
        GradientKey<Vector4>{0.3f, {1.f, 0.55f, 0.1f, 1.f}},
        GradientKey<Vector4>{1.f,  {0.6f, 0.05f, 0.f, 0.f}}
    };
    config.sizeKeys = {
        GradientKey<Vector3>{0.f,  {0.15f, 0.15f, 0.15f}},
        GradientKey<Vector3>{0.2f, {0.35f, 0.35f, 0.35f}},
        GradientKey<Vector3>{1.f,  {0.02f, 0.02f, 0.02f}}
    };

    // 再入場での多重登録を避けるため、常に上書きで登録する
    ParticleSystem::Template tmpl;
    tmpl.emitters.push_back(config);
    Particle()->Register(kLaserTemplateName, tmpl, true);
}

void ParticleTest::Update() {
    const auto input = Singleton<Input>::GetInstance();

    Vector3 move{};
    if (input->IsPress(DIK_A) || input->IsPress(DIK_LEFT))  move.x -= 1.f;
    if (input->IsPress(DIK_D) || input->IsPress(DIK_RIGHT)) move.x += 1.f;
    if (input->IsPress(DIK_S) || input->IsPress(DIK_DOWN))  move.z -= 1.f;
    if (input->IsPress(DIK_W) || input->IsPress(DIK_UP))    move.z += 1.f;

    laserPosition_ += move * kMoveSpeed * Time::GetDeltaTime();

    const bool fireHeld = input->IsPress(DIK_SPACE);
    if (fireHeld && !firing_) {
        laserHandle_ = Particle()->Emit(kLaserTemplateName, laserPosition_);
    } else if (!fireHeld && firing_) {
        laserHandle_.Stop();
    }
    firing_ = fireHeld;

    laserHandle_.SetPosition(laserPosition_);
}

void ParticleTest::Draw() { }
