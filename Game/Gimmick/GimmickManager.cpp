#define NOMINMAX
#include "GimmickManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <variant>

#include "Gimmick/RouteGimmick.hpp"
#include "Gimmick/TowerDefenseGimmick.hpp"
#include "Gimmick/TowerOrbitGimmick.hpp"
#include "Json/JsonParams.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Sound/GameSound.hpp"
#include "Tower/MainTower.hpp"
#include "Tower/Tower.hpp"
#include "Tower/TowerManager.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

#ifdef _DEBUG
#include "imgui.h"
#endif

#undef min
#undef max

namespace {
    const std::string WHITE_TEXTURE = "white_x16.png";

    /// 失敗時の爆発テンプレート(Assets/Data/Particle/GimmickFailed.json)。
    /// 起動時に自動で読み込まれ、DebugUI の Particle エディタから編集・保存できる
    const std::string FAILURE_TEMPLATE = "GimmickFailed";

    /// テンプレートの JSON から参照される関数のキー
    const std::string FAILURE_BURST_SPAWN = "GimmickFailed.Burst";
    const std::string FAILURE_SMOKE_SPAWN = "GimmickFailed.Smoke";
    const std::string FAILURE_FALL_UPDATE = "GimmickFailed.Fall";
    const std::string FAILURE_DRIFT_UPDATE = "GimmickFailed.Drift";

    Vector4 WithOpacity(Vector4 _color, float _opacity) {
        _color.w *= _opacity;
        return _color;
    }
}

void GimmickManager::Initialize(const GimmickContext& _context) {
    context_ = _context;
    activeGimmick_.reset();
    spawnTime_ = 0.0f;
    remainingTimeSeconds_ = 0.0f;
    lastRandomGimmick_.reset();
    LoadConfig();
    RegisterFailureEffect();
    InitializeTimerGauge();
    RouteGimmick::ResetInvocationCount();
}

void GimmickManager::Update(float _deltaTime) {
    if (pendingStart_) {
        const GimmickType type = *pendingStart_;
        pendingStart_.reset();
        StartGimmick(type);
    }

    // ポーズやリザルトで進行が止まっている間は、回転ギミックのループ音を鳴らしっぱなしにしない。
    // ギミック側の Update が呼ばれなくなるので、止めるのはここの役目になる
    if (!std::isfinite(_deltaTime) || _deltaTime <= 0.0f || debugPaused_) {
        GameSound::StopLoop(GameSound::Se::GimmickOrbitLoop);
        return;
    }

    if (activeGimmick_) {
        activeGimmick_->Update(_deltaTime);
        if (!activeGimmick_->IsFinished()) {
            remainingTimeSeconds_ = std::max(remainingTimeSeconds_ - _deltaTime, 0.0f);
            if (remainingTimeSeconds_ <= 0.0f) activeGimmick_->OnTimeLimitExpired();
        }
        if (activeGimmick_->IsFinished()) {
            if (activeGimmick_->GetState() == GimmickState::Failed) OnGimmickFailed();
            else GameSound::Play(GameSound::Se::GimmickClear);
            // 回っている途中で終わった場合に備えて、ループ音はここでも止めておく
            GameSound::StopLoop(GameSound::Se::GimmickOrbitLoop);
            activeGimmick_.reset();
            remainingTimeSeconds_ = 0.0f;
            spawnTime_ = 0.0f;
        }
        UpdateTimerGauge();
        return;
    }

    spawnTime_ += _deltaTime;
    if (spawnTime_ < spawnInterval_) return;
    spawnTime_ = 0.0f;
    StartRandomGimmick();
}

void GimmickManager::Draw() {
    if (activeGimmick_) activeGimmick_->Draw();
    if (!activeGimmick_ || !visible_) return;
    timerGaugeFrame_.Draw();
    timerGaugeFill_.Draw();
    timerLabelText_.Draw();
    timerValueText_.Draw();
}

void GimmickManager::Debug() {
#ifdef _DEBUG
    ImGui::Begin("GimmickManager");
    ImGui::Text("Active: %s", activeGimmick_ ? "Yes" : "No");
    if (activeGimmick_) {
        ImGui::Text("Remaining: %.2f / %.2f", remainingTimeSeconds_, timeLimitSeconds_);
    }

    if (ImGui::Button("Route")) pendingStart_ = GimmickType::Route;
    ImGui::SameLine();
    if (ImGui::Button("TowerDefense")) pendingStart_ = GimmickType::TowerDefense;
    ImGui::SameLine();
    if (ImGui::Button("TowerOrbit")) pendingStart_ = GimmickType::TowerOrbit;

    if (activeGimmick_) {
        ImGui::Checkbox("Paused", &debugPaused_);
        ImGui::SameLine();
        if (ImGui::Button("Restart")) pendingStart_ = activeGimmick_->GetType();
    }
    ImGui::End();

    if (activeGimmick_) activeGimmick_->Debug();
#endif
}

void GimmickManager::RegisterFailureEffect() {
    if (!context_.particleSystem) return;

    // 破片は球状にばらけさせる。上向きを少し強めにして「吹き上がる」形にする
    context_.particleSystem->RegisterSpawnFunc(FAILURE_BURST_SPAWN,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float yaw = MathUtils::Random(0.0f, 6.2831853f);
            const float pitch = MathUtils::Random(-0.35f, 1.15f);
            const float horizontal = std::cos(pitch);
            const Vector3 direction{
                std::cos(yaw) * horizontal, std::sin(pitch), std::sin(yaw) * horizontal};
            _position = _center + direction * MathUtils::Random(0.0f, 0.6f);
            _velocity = direction * MathUtils::Random(7.0f, 18.0f);
        });

    // 煙はゆっくり上へ。破片が消えたあとに残って失敗の跡になる。
    // 初速を抑えて Drift で減速させると、勢いよく上がらず「ふわっと漂う」動きになる
    context_.particleSystem->RegisterSpawnFunc(FAILURE_SMOKE_SPAWN,
        [](const Vector3& _center, Vector3& _position, Vector3& _velocity) {
            const float yaw = MathUtils::Random(0.0f, 6.2831853f);
            const Vector3 spread{std::cos(yaw), 0.0f, std::sin(yaw)};
            _position = _center + spread * MathUtils::Random(0.0f, 1.8f)
                + Vector3{0.0f, MathUtils::Random(0.0f, 1.0f), 0.0f};
            _velocity = spread * MathUtils::Random(0.15f, 0.55f)
                + Vector3{0.0f, MathUtils::Random(0.6f, 1.5f), 0.0f};
        });

    // 煙は重力を掛けず、空気抵抗だけで少しずつ止まっていく
    context_.particleSystem->RegisterUpdateFunc(FAILURE_DRIFT_UPDATE,
        [](float, const Vector3&, Vector3&, Vector3& _velocity, Vector4&) {
            constexpr float STEP = 1.0f / 60.0f;
            constexpr float DRAG = 0.7f;
            const float drag = DRAG * STEP;
            _velocity.x -= _velocity.x * drag;
            _velocity.y -= _velocity.y * drag;
            _velocity.z -= _velocity.z * drag;
        });

    // 破片だけは重力と空気抵抗を掛けて落とす。
    // @note Particle の積分は 1/60 秒固定なので、ここでも同じ刻みで速度を変える
    context_.particleSystem->RegisterUpdateFunc(FAILURE_FALL_UPDATE,
        [](float, const Vector3&, Vector3&, Vector3& _velocity, Vector4&) {
            constexpr float STEP = 1.0f / 60.0f;
            constexpr float GRAVITY = 26.0f;
            constexpr float DRAG = 1.8f;
            _velocity.y -= GRAVITY * STEP;
            const float drag = DRAG * STEP;
            _velocity.x -= _velocity.x * drag;
            _velocity.y -= _velocity.y * drag;
            _velocity.z -= _velocity.z * drag;
        });
}

void GimmickManager::OnGimmickFailed() {
    // 失敗の代償はメインタワーの HP。被弾フラッシュと効果音は TakeDamage が鳴らす
    if (context_.towerManager) {
        // 既定の被弾音は鳴らさない。爆発音と重ねると何が起きたのか読み取りにくくなる
        context_.towerManager->TakeDamage(failureTowerDamage_, false);
    }
    GameSound::Play(GameSound::Se::GimmickTowerDamage);

    if (!context_.particleSystem || !context_.towerManager) return;

    // 爆発はダメージを受けた防衛対象の上で出す。
    // damage と演出の場所を揃えて「守れなかったからタワーが傷んだ」と読めるようにする
    const auto& mainTowers = context_.towerManager->GetMainTowers();
    if (mainTowers.empty() || !mainTowers.front()) return;

    context_.particleSystem->Emit(FAILURE_TEMPLATE,
        mainTowers.front()->GetPosition() + Vector3{0.0f, failureEffectHeight_, 0.0f});
}

void GimmickManager::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Gimmick", "Gimmick")) return;
    const auto groups = json->GetGroups("Gimmick");

    const auto read = [](const auto& _group, const char* _key, float _fallback) {
        const auto entry = _group.find(_key);
        if (entry == _group.end()) return _fallback;
        if (const auto floatValue = std::get_if<float>(&entry->second)) return *floatValue;
        if (const auto integerValue = std::get_if<int32_t>(&entry->second)) {
            return static_cast<float>(*integerValue);
        }
        return _fallback;
    };

    if (const auto spawn = groups.find("Spawn"); spawn != groups.end()) {
        spawnInterval_ = read(spawn->second, "IntervalSeconds", spawnInterval_);
    }
    if (const auto failure = groups.find("Failure"); failure != groups.end()) {
        failureTowerDamage_ = read(failure->second, "TowerDamage", failureTowerDamage_);
        failureEffectHeight_ = read(failure->second, "EffectHeight", failureEffectHeight_);
    }
    if (const auto lottery = groups.find("Lottery"); lottery != groups.end()) {
        routeWeight_ = read(lottery->second, "RouteWeight", routeWeight_);
        towerDefenseWeight_ = read(
            lottery->second, "TowerDefenseWeight", towerDefenseWeight_);
        towerOrbitWeight_ = read(lottery->second, "TowerOrbitWeight", towerOrbitWeight_);
    }
    if (const auto gauge = groups.find("TimerGauge"); gauge != groups.end()) {
        const auto readValue = []<typename T>(
            const auto& _group, const char* _key, const T& _fallback) {
            const auto entry = _group.find(_key);
            if (entry == _group.end()) return _fallback;
            if (const auto value = std::get_if<T>(&entry->second)) return *value;
            return _fallback;
        };
        timerGaugePosition_ = readValue(gauge->second, "Position", timerGaugePosition_);
        timerGaugeSize_ = readValue(gauge->second, "Size", timerGaugeSize_);
        timerGaugeFrameThickness_ = read(
            gauge->second, "FrameThickness", timerGaugeFrameThickness_);
        timerGaugeFrameColor_ = readValue(
            gauge->second, "FrameColor", timerGaugeFrameColor_);
        timerGaugeColor_ = readValue(gauge->second, "Color", timerGaugeColor_);
        timerLabel_ = readValue(gauge->second, "Label", timerLabel_);
        timerLabelPosition_ = readValue(
            gauge->second, "LabelPosition", timerLabelPosition_);
        timerLabelFontSize_ = read(gauge->second, "LabelFontSize", timerLabelFontSize_);
        timerLabelColor_ = readValue(gauge->second, "LabelColor", timerLabelColor_);
        timerValueRightX_ = read(gauge->second, "ValueRightX", timerValueRightX_);
        timerValuePositionY_ = read(
            gauge->second, "ValuePositionY", timerValuePositionY_);
        timerValueFontSize_ = read(
            gauge->second, "ValueFontSize", timerValueFontSize_);
    }

    spawnInterval_ = std::isfinite(spawnInterval_) ? std::max(spawnInterval_, 0.0f) : 30.0f;
    routeWeight_ = std::isfinite(routeWeight_) ? std::max(routeWeight_, 0.0f) : 1.0f;
    towerDefenseWeight_ = std::isfinite(towerDefenseWeight_)
        ? std::max(towerDefenseWeight_, 0.0f) : 1.0f;
    towerOrbitWeight_ = std::isfinite(towerOrbitWeight_)
        ? std::max(towerOrbitWeight_, 0.0f) : 1.0f;
}

void GimmickManager::StartRandomGimmick() {
    // 前回選ばれた種類だけ今回の候補から外し、同じギミックの連続発生を防ぐ。
    const float routeWeight = lastRandomGimmick_ != GimmickType::Route
        ? routeWeight_ : 0.0f;
    const float towerDefenseWeight = lastRandomGimmick_ != GimmickType::TowerDefense
        ? towerDefenseWeight_ : 0.0f;
    const float towerOrbitWeight = lastRandomGimmick_ != GimmickType::TowerOrbit
        ? towerOrbitWeight_ : 0.0f;
    const float totalWeight = routeWeight + towerDefenseWeight + towerOrbitWeight;
    if (totalWeight <= 0.0f) return;

    const float lottery = MathUtils::Random(0.0f, totalWeight);
    GimmickType type = GimmickType::TowerOrbit;
    if (lottery < routeWeight) {
        type = GimmickType::Route;
    } else if (lottery < routeWeight + towerDefenseWeight) {
        type = GimmickType::TowerDefense;
    }

    lastRandomGimmick_ = type;
    StartGimmick(type);
}

void GimmickManager::StartGimmick(GimmickType _type) {
    activeGimmick_ = CreateGimmick(_type);
    if (activeGimmick_) {
        activeGimmick_->Initialize(context_);
        // 対象のタワーが見つからないなどで初期化した時点で終わっている場合は、
        // 発生しなかったものとして捨てる。ここを通さないと達成音や失敗の爆発が空振りする
        if (activeGimmick_->IsFinished()) {
            activeGimmick_.reset();
            spawnTime_ = 0.0f;
            return;
        }
        timeLimitSeconds_ = activeGimmick_->GetTimeLimitSeconds();
        if (!std::isfinite(timeLimitSeconds_) || timeLimitSeconds_ <= 0.0f) {
            timeLimitSeconds_ = 10.0f;
        }
        remainingTimeSeconds_ = timeLimitSeconds_;
        UpdateTimerGauge();
    }
    spawnTime_ = 0.0f;
}

std::unique_ptr<IGimmick> GimmickManager::CreateGimmick(GimmickType _type) const {
    switch (_type) {
    case GimmickType::Route:
        return std::make_unique<RouteGimmick>();
    case GimmickType::TowerDefense:
        return std::make_unique<TowerDefenseGimmick>();
    case GimmickType::TowerOrbit:
        return std::make_unique<TowerOrbitGimmick>();
    }
    return nullptr;
}

void GimmickManager::InitializeTimerGauge() {
    for (Sprite* sprite : {&timerGaugeFrame_, &timerGaugeFill_}) {
        sprite->Initialize(WHITE_TEXTURE);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }
    timerLabelText_.Initialize(
        timerLabel_, timerLabelPosition_.x, timerLabelPosition_.y, timerLabelFontSize_);
    timerValueText_.Initialize("", timerValueRightX_, timerValuePositionY_, timerValueFontSize_);
    UpdateTimerGauge();
}

void GimmickManager::UpdateTimerGauge() {
    const float ratio = timeLimitSeconds_ > 0.0f
        ? std::clamp(remainingTimeSeconds_ / timeLimitSeconds_, 0.0f, 1.0f)
        : 0.0f;
    timerGaugeFrame_.SetPosition({
        timerGaugePosition_.x - timerGaugeFrameThickness_, timerGaugePosition_.y});
    timerGaugeFrame_.SetSize({
        timerGaugeSize_.x + timerGaugeFrameThickness_ * 2.0f,
        timerGaugeSize_.y + timerGaugeFrameThickness_ * 2.0f});
    timerGaugeFrame_.SetColor(WithOpacity(timerGaugeFrameColor_, opacity_));
    timerGaugeFrame_.Update();

    timerGaugeFill_.SetPosition(timerGaugePosition_);
    timerGaugeFill_.SetSize({timerGaugeSize_.x * ratio, timerGaugeSize_.y});
    timerGaugeFill_.SetColor(WithOpacity(timerGaugeColor_, opacity_));
    timerGaugeFill_.Update();

    timerLabelText_.SetColor(WithOpacity(timerLabelColor_, opacity_));
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.1fs", remainingTimeSeconds_);
    const std::string value = buffer;
    timerValueText_.SetText(value);
    timerValueText_.SetPosition(
        timerValueRightX_
            - static_cast<float>(value.size()) * timerValueFontSize_ * timerValueCharWidthRatio_,
        timerValuePositionY_);
    timerValueText_.SetColor(WithOpacity(timerGaugeColor_, opacity_));
}

void GimmickManager::SetVisible(bool _visible) {
    visible_ = _visible;
    timerLabelText_.SetVisible(_visible);
    timerValueText_.SetVisible(_visible);
}

void GimmickManager::SetOpacity(float _opacity) {
    opacity_ = std::clamp(_opacity, 0.0f, 1.0f);
    UpdateTimerGauge();
}

bool GimmickManager::GetIndicatorPosition(Vector3& _position) const {
    return activeGimmick_ && activeGimmick_->GetIndicatorPosition(_position);
}
