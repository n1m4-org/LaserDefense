#ifndef UI_ANIM_PRESETS_HPP_
#define UI_ANIM_PRESETS_HPP_

namespace Ui { class Canvas; }

/// UIエディタ(Canvas)のドロップダウンから選べるアニメーションをまとめた場所
///
/// ShowAnim / IdleAnim / HideAnim の候補に出るのは
/// Canvas::RegisterAnimFunc() で登録したものだけなので、
/// 画面ごとに同じ関数を書き直さずに済むよう共通のものはここへ置く。
namespace UiAnimPresets {

    /// @brief 共通のアニメーションをまとめて Canvas へ登録する
    ///
    /// 登録されるキーは以下の3つ。
    ///   FadeIn … 少し下から浮き上がりながらフェードイン(ShowAnim向け)
    ///   Blink  … ゆっくり明滅を繰り返す(IdleAnim向け)
    ///   Float  … 上下にふわふわ揺れ続ける(IdleAnim向け)
    ///
    /// @note Canvas::Setup() の中で JSON に書かれたキーが解決されるため、
    ///       必ず Setup() より先に呼ぶこと。後から登録しても再生されない
    void RegisterAll(Ui::Canvas& _canvas);

} // namespace UiAnimPresets

#endif // UI_ANIM_PRESETS_HPP_
