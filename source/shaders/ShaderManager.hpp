#pragma once
#include <string>
#include <map>
#include <vector>
#include <citro2d.h>
#include <citro3d.h>
#include <set>

struct ShaderParams {
    std::string name = "";
    float value1 = 1.0f;
    float value2 = 0.0f;
    float value3 = 0.0f;
};

// A stack of up to N shaders applied in order to a single camera
using ShaderStack = std::vector<ShaderParams>;

class ShaderManager {
public:
    static ShaderManager& get() {
        static ShaderManager instance;
        return instance;
    }

    void init();
    void cleanup();

    // Lua bindings
    // Adds shader to camera stack (updates params if name already exists, otherwise appends)
    void setCameraShader(const std::string& camera, const std::string& shaderName, float v1 = 1.0f, float v2 = 0.0f, float v3 = 0.0f);
    // Modify param of the FIRST shader in stack (backward compat)
    void setShaderFloat(const std::string& camera, int index, float value);
    // Modify param of a specific named shader in stack
    void setShaderParam(const std::string& camera, const std::string& shaderName, int index, float value);
    // Remove all shaders from camera
    void removeCameraShader(const std::string& camera);
    // Remove specific named shaders from camera stack
    void removeCameraShaders(const std::string& camera, const std::vector<std::string>& names);

    // Engine integration
    bool beginCamera(const std::string& camera, C3D_RenderTarget* fallbackTarget);
    void endCamera(const std::string& camera, C3D_RenderTarget* top, C3D_RenderTarget* bottom = nullptr);
    void update(float dt);
    void clearAllShaders();

    std::set<std::string> extendedCameras;
    void setCameraExtended(const std::string& camera, bool extended);
    bool isCameraExtended(const std::string& camera) const;

private:
    ShaderManager();
    ~ShaderManager();

    std::map<std::string, ShaderStack> activeShaders;

    struct RT {
        C3D_Tex tex;
        C3D_RenderTarget* target = nullptr;
        C2D_Image img;
        Tex3DS_SubTexture* origSubtex = nullptr;
        bool active = false;
        void init(int w, int h);
        void cleanup();
    };

    std::map<std::string, RT> targets;
    RT helperRT;   // Internal scratch buffer for complex shaders (pixelate, drugs, bw)
    RT helperRT2;  // Chain buffer for multi-shader stacking (source for 2nd shader in stack)

    std::map<std::string, std::pair<float, float>> scrollAccumulators;
    u64 lastTick = 0;

    void presentCamera(const std::string& camName, C3D_RenderTarget* dest, bool isBottomPart = false);
    void drawShaderEffect(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, const ShaderParams& params, float x, float y, float scaleX, float scaleY);
    
    // Shader implementations
    void drawTevTint(const RT& rt, C3D_RenderTarget* dest, C2D_TintMode mode, u32 color, float blend, float x, float y, float scaleX, float scaleY);
    void drawSaturation(const RT& rt, C3D_RenderTarget* dest, float saturation, float x, float y, float scaleX, float scaleY);
    void drawChromatic(const RT& rt, C3D_RenderTarget* dest, float offset, float x, float y, float scaleX, float scaleY);
    void drawWave(const RT& rt, C3D_RenderTarget* dest, float speed, float xStrength, float yStrength, float x, float y, float scaleX, float scaleY);
    void drawGlitchSkew(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float stripH, float x, float y, float scaleX, float scaleY);
    void drawCRT(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY);
    void drawBlur(const RT& rt, C3D_RenderTarget* dest, float radius, float x, float y, float scaleX, float scaleY);
    void drawPixelate(const RT& rt, C3D_RenderTarget* dest, float pixelSize, float x, float y, float scaleX, float scaleY);
    void drawTiling(const RT& rt, C3D_RenderTarget* dest, float count, float x, float y, float scaleX, float scaleY);
    void drawVignette(const RT& rt, C3D_RenderTarget* dest, float opacity, float size, float x, float y, float scaleX, float scaleY);
    void drawMirror(const RT& rt, C3D_RenderTarget* dest, float type, float x, float y, float scaleX, float scaleY);
    void drawScanlineRoll(const RT& rt, C3D_RenderTarget* dest, float speed, float opacity, float x, float y, float scaleX, float scaleY);
    void drawVHS(const RT& rt, C3D_RenderTarget* dest, float strength, float x, float y, float scaleX, float scaleY);
    void drawColorDepth(const RT& rt, C3D_RenderTarget* dest, float levels, float x, float y, float scaleX, float scaleY);
    void drawScroll(const std::string& camera, const RT& rt, C3D_RenderTarget* dest, float speedX, float speedY, float zoom, float x, float y, float scaleX, float scaleY);
    void drawDrugs(const RT& rt, C3D_RenderTarget* dest, float speed, float strength, float colorSpeed, float x, float y, float scaleX, float scaleY);
    void drawBW(const RT& rt, C3D_RenderTarget* dest, float passes, float x, float y, float scaleX, float scaleY);
    
    // Vertex grid drawing (for wave/bulge/crt)
    void drawMesh(const RT& rt, C3D_RenderTarget* dest, const ShaderParams& params, float x, float y, float scaleX, float scaleY);

    Tex3DS_SubTexture tempSubtexs[512];
};
