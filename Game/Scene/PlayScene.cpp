#include "PlayScene.hpp"

#include <array>
#include <cmath>

#include "Camera/Controller/CameraController.hpp"
#include "Camera/PlayerCamera.hpp"
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
    constexpr Vector3 mainTowerPosition{0.0f, 0.0f, 0.0f};
    constexpr std::array<Vector3, 8> towerPositions{{
        {-35.0f, 0.0f, -35.0f}, {0.0f, 0.0f, -35.0f}, {35.0f, 0.0f, -35.0f},
        {-35.0f, 0.0f, 0.0f}, {35.0f, 0.0f, 0.0f},
        {-35.0f, 0.0f, 35.0f}, {0.0f, 0.0f, 35.0f}, {35.0f, 0.0f, 35.0f}
    }};
    constexpr Vector3 shadowLightOffset{0.0f, 10.0f, 0.0f};

    Singleton<TextureManager>::GetInstance()->Load("skybox.dds");

    player_ = std::make_unique<Player>();
    player_->Initialize();
    // 画面手前（-Z）に出現。高さは床と同じ0。
    player_->SetPosition(mainTowerPosition + Vector3{0.0f, 0.0f, -8.0f});
    player_->SetInput(input_);
    player_->EnableGrappleMovement();
    playerCamera_ = std::make_unique<PlayerCamera>();
    playerCamera_->Initialize(*player_);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);

    towerManager_ = std::make_unique<TowerManager>();
    towerManager_->Initialize();
    towerManager_->AddMainTower(mainTowerPosition);
    for (const Vector3& position : towerPositions) {
        towerManager_->AddTower(position);
    }

    laser_ = std::make_unique<Laser>();
    laser_->Initialize(Particle());
    laser_->SetStart(player_.get(), player_->GetModelOffset().y);
    laser_->ClearTarget();

    // スコアと制限時間は EnemyManager より先に用意し、撃破報酬の加算先として渡しておく
    scoreManager_ = std::make_unique<ScoreManager>();
    scoreManager_->Initialize();

    timeLimitManager_ = std::make_unique<TimeLimitManager>();
    timeLimitManager_->Initialize();

    enemyManager_ = std::make_unique<EnemyManager>();
    enemyManager_->Initialize();
    enemyManager_->SetTargetPosition(mainTowerPosition.x, mainTowerPosition.z);
    enemyManager_->SetScoreManager(scoreManager_.get());
    enemyManager_->SetTimeLimitManager(timeLimitManager_.get());

    floor_ = std::make_unique<Model>();
    floor_->Initialize("plane");
    floor_->SetTexture("white_x16.png");
    floor_->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    floor_->SetTranslate({0.0f, 0.0f, 0.0f});
    floor_->SetRotate({-1.5707963f, 0.0f, 0.0f});
    floor_->SetScale({50.0f, 50.0f, 1.0f});

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
    // 選択判定・カーソル・描画に同じカメラ行列を使う。
    playerCamera_->Update(*player_, deltaTime);
    towerManager_->Update(deltaTime);
    UpdateTowerSelection();
    player_->SetGrappleTarget(laser_->GetConnectedTarget());
    player_->Update(deltaTime);
    Singleton<LightManager>::GetInstance()->SetPosition(
        player_->GetPosition() + shadowLightOffset);
    enemyManager_->Update(deltaTime);
    laser_->Update();
    scoreManager_->Update(deltaTime);
    timeLimitManager_->Update(deltaTime);
    floor_->Update();
}

void PlayScene::Draw() {
    player_->Draw();
    enemyManager_->Draw();
    towerManager_->Draw();
    laser_->Draw();
    floor_->Draw();


    // UI は 3D の描画がすべて終わったあとに重ねる

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
    const bool hasFocus = foregroundProcess == GetCurrentProcessId();
    mouseAvailable = mouseAvailable && hasFocus;
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
    // カーソルがSceneやタワーから外れても保持。ボタン解放またはフォーカス喪失で解除。
    if (!hasFocus || !mouse->IsMousePress(0)) {
        laser_->ClearTarget();
    } else if (mouseAvailable && mouse->IsMouseTrigger(0)) {
        // 押し始めたTowerを保持。ドラッグで別Towerへ乗り換えない。
        if (hovered) laser_->SetTarget(hovered);
        else laser_->ClearTarget();
    }

    scoreManager_->Draw();
    timeLimitManager_->Draw();
}
