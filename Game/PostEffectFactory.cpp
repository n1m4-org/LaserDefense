#include "Factory/PostEffectFactory.hpp"
#include "Utils.hpp"
#include "PostProcess/BoxBlur/BoxBlur.hpp"
#include "PostProcess/Grayscale/Grayscale.hpp"
#include "PostProcess/Vignette/Vignette.hpp"

std::unique_ptr<IPostEffect> PostEffectFactory::Create(const std::string& _type) {
    std::unique_ptr<IPostEffect> effect = nullptr;

    if (Utils::EqualsIgnoreCase(_type, "Vignette")) {
        effect = std::make_unique<Vignette>();
    }
    else if (Utils::EqualsIgnoreCase(_type, "Grayscale")) {
        effect = std::make_unique<Grayscale>();
    }
    else if (Utils::EqualsIgnoreCase(_type, "BoxBlur")) {
        effect = std::make_unique<BoxBlur>();
    }
    // else if (Utils::EqualsIgnoreCase(type, "Bloom")) {
    //     effect = std::make_unique<Bloom>();
    // }
    else {
        Utils::Alert(std::format("PostEffectFactory::Create: Effect type '{}' not found", _type));
        return nullptr;
    }

    return effect;
}
