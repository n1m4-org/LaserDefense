#ifndef PostEffectFactory_HPP_
#define PostEffectFactory_HPP_
#include "Factory/AbstractPostEffectFactory.hpp"

/// <summary>
/// PostEffect生成用のファクトリークラス
/// ゲーム固有のPostEffectタイプを生成する
/// </summary>
class PostEffectFactory final : public AbstractPostEffectFactory {
public:
    PostEffectFactory() = default;
    ~PostEffectFactory() override = default;

    /// <summary>
    /// PostEffectを生成
    /// </summary>
    /// <param name="_type">エフェクトタイプ名</param>
    /// <returns>生成されたPostEffectのユニークポインタ</returns>
    std::unique_ptr<IPostEffect> Create(const std::string& _type) override;
}; // class PostEffectFactory

#endif // PostEffectFactory_HPP_
