#ifndef IPAUSERENDERER_HPP
#define IPAUSERENDERER_HPP

#include <string>
#include <vector>
#include <citro2d.h>

class PauseSubState;

class IPauseRenderer {
public:
    virtual ~IPauseRenderer() = default;

    virtual void init() = 0;
    virtual void onMenuChanged(const std::vector<std::string>& items, const std::string& title) = 0;
    virtual void update(float dt) = 0;
    virtual void drawTop(C3D_RenderTarget* target, float screenW, float screenH) = 0;
    virtual void drawBottom(C3D_RenderTarget* target, float screenW, float screenH) = 0;

    virtual void onCursorMove() {}
    virtual void onSelect() {}
    virtual void onCancel() {}
};

#endif

