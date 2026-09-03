#include "PlayScene.hpp"

#include <array>
#include <cmath>

#include "Camera/Controller/CameraController.hpp"
#include "GameObject/Player/Player.h"
#include "Laser/Laser.hpp"
#include "Light/LightManager.hpp"
#include "Input.hpp"
#include "Line.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"
#include "Texture/TextureManager.hpp"
#include "Time/Time.hpp"

#ifdef _DEBUG
#include "imgui_internal.h"
#endif

PlayScene::PlayScene() = default;
PlayScene::~PlayScene() = default;

void PlayScene::Initialize() {
    constexpr std::array<Vector3, 4> towerPositions{{
        {-15.0f, 0.0f, -15.0f}, {15.0f, 0.0f, -15.0f},
        {-15.0f, 0.0f, 15.0f}, {15.0f, 0.0f, 15.0f}
    }};
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    if (const auto camera = Singleton<CameraController>::GetInstance()->GetActive()) {
        camera->transform_.translate = {0.0f, 65.0f, -45.0f};
        camera->transform_.rotate = Vector3{std::atan2(65.0f, 45.0f), 0.0f, 0.0f};
        camera->SetFov(1.0f);
        camera->SetFar(200.0f);
        camera->Update();
    }

    player_ = std::make_unique<Player>();
    player_->Initialize();
    player_->SetInput(input_);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();
    for (const Vector3& position : towerPositions) {
        towerManager_->AddTower(position);
    }

    laser_ = std::make_unique<Laser>();
    laser_->Initialize(Particle());
    laser_->SetStart(player_.get(), player_->GetModelOffset().y);
    laser_->ClearTarget();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
    enemyManager_->SetTargetPosition(towerPositions.front().x, towerPositions.front().z);

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTexture("white_x16.png");
    floor_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({25.0f, 25.0f, 1.0f});

    mouseCursor_ = std::make_unique<Line>();
    mouseCursor_->Initialize();
    mouseCursor_->SetName("MouseCursor");
    mouseCursor_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    cursorVisible_ = false;
}

void PlayScene::Update() {
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    input_.Update();

    const float deltaTime = Time::GetDeltaTime();
    player_->Update(deltaTime);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);
    enemyManager_->Update(deltaTime);
    towerManager_->Update(deltaTime);
    UpdateTowerSelection();
    laser_->Update();
    floor_->Update();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();
    if (cursorVisible_) mouseCursor_->Draw();
}

void PlayScene::UpdateTowerSelection() {
    const auto mouse = Singleton<Input>::GetInstance();
    const auto screen = Singleton<Screen>::GetInstance();
    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    const Vector2 position = mouse->GetMousePosition();
    const float width = screen->Width();
    const float height = screen->Height();

    bool mouseAvailable = camera && width > 0.0f && height > 0.0f
        && position.x >= 0.0f && position.x < width
        && position.y >= 0.0f && position.y < height;
    // フォーカス喪失時も接続を解除する（Input側が前フレーム値を保持する場合への対策）。
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    mouseAvailable = mouseAvailable && foregroundProcess == GetCurrentProcessId();
#ifdef _DEBUG
    if (const auto* context = ImGui::GetCurrentContext(); context && context->HoveredWindow) {
        mouseAvailable = mouseAvailable && std::string(context->HoveredWindow->Name) == "Scene";
    }
#endif

    mouseCursor_->Clear();
    cursorVisible_ = mouseAvailable;
    Tower* hovered = nullptr;
    if (mouseAvailable) {
        const Matrix4x4 inverseViewProjection = camera->GetViewProjection().Inverse();
        const auto unproject = [&](float _x, float _y, float _depth) {
            return MathUtils::Matrix::Transform(
                Vector3{2.0f * _x / width - 1.0f, 1.0f - 2.0f * _y / height, _depth},
                inverseViewProjection);
        };
        const Vector3 nearPosition = unproject(position.x, position.y, 0.0f);
        const Vector3 farPosition = unproject(position.x, position.y, 1.0f);
        const Vector3 rayDirection = farPosition - nearPosition;
        hovered = towerManager_->PickTower(nearPosition, rayDirection.Normalize(), rayDirection.Length());

        // スクリーン上で半径5pxの丸を、手前の平面に逆投影して描画する。
        constexpr int segments = 24;
        constexpr float radius = 5.0f;
        for (int i = 0; i < segments; ++i) {
            const float a = 2.0f * MathUtils::F_PI * static_cast<float>(i) / segments;
            const float b = 2.0f * MathUtils::F_PI * static_cast<float>(i + 1) / segments;
            mouseCursor_->AddLine(
                unproject(position.x + std::cos(a) * radius, position.y + std::sin(a) * radius, 0.01f),
                unproject(position.x + std::cos(b) * radius, position.y + std::sin(b) * radius, 0.01f));
        }
        mouseCursor_->Update();
    }

    towerManager_->SetHoveredTower(hovered);
    if (!mouseAvailable || !mouse->IsMousePress(0)) {
        laser_->ClearTarget();
    } else if (mouse->IsMouseTrigger(0)) {
        // 押し始めたTowerを保持。ドラッグで別Towerへ乗り換えない。
        if (hovered) laser_->SetTarget(hovered);
        else laser_->ClearTarget();
    }
}
