#pragma once
#include "IScene.hpp"
#include "Math/Vector3.hpp"
#include "src/ParticleSystem/Emitter/Emitter.hpp"

class ParticleTest : public IScene {

public:
    void Initialize() override;
    void Update() override;
    void Draw() override;

private:
    void RegisterLaserTemplate();

    Vector3 laserPosition_{0.f, 0.f, 0.f};
    EmitterHandle laserHandle_;
    bool firing_ = false;
};
