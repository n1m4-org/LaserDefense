#define NOMINMAX
#include "UiAnimPresets.hpp"

#include <algorithm>
#include <cmath>

#include "Ui/UserInterface.hpp"

namespace {
    /// FadeIn が終わるまでの秒数
    constexpr float FADE_IN_DURATION = 0.4f;
    /// FadeIn で何ピクセル下から浮き上がるか
    constexpr float FADE_IN_RISE = 24.0f;

    /// 下から浮き上がりながら現れる
    bool FadeIn(float _elapsed, Vector2& _posOffset, Vector4& _color) {
        const float t = std::clamp(_elapsed / FADE_IN_DURATION, 0.0f, 1.0f);
        _color.w *= t;
        _posOffset.y += (1.0f - t) * FADE_IN_RISE;
        return t >= 1.0f;
    }

    /// 明滅を繰り返す。終わらないので IdleAnim 専用
    bool Blink(float _elapsed, Vector2&, Vector4& _color) {
        const float wave = (std::sin(_elapsed * 3.0f) + 1.0f) * 0.5f;
        _color.w *= 0.35f + wave * 0.65f;
        return false;
    }

    /// 上下にゆっくり揺れ続ける。終わらないので IdleAnim 専用
    bool FloatLoop(float _elapsed, Vector2& _posOffset, Vector4&) {
        _posOffset.y += std::sin(_elapsed * 1.6f) * 4.0f;
        return false;
    }
} // namespace

namespace UiAnimPresets {

    void RegisterAll(Ui::Canvas& _canvas) {
        _canvas.RegisterAnimFunc("FadeIn", FadeIn);
        _canvas.RegisterAnimFunc("Blink", Blink);
        _canvas.RegisterAnimFunc("Float", FloatLoop);
    }

} // namespace UiAnimPresets
