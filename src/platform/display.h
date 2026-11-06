#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>

#include "core/point.h"
#include "core/types.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Cursor;
struct SDL_Rect;
union SDL_Event;

namespace swd3e
{

class Surface;
struct IconImage;

// Where the logical screen is drawn inside the render output (in output pixels).
struct Viewport
{
    float x;
    float y;
    float width;
    float height;
};

struct LogicalPoint
{
    int x;
    int y;
};

// What the window shows: a canvas of width x height logical pixels, and where the game's 640x480 screen lies on it
// (its left and top), which the pointer's coordinates count from. The map fills the window with a larger canvas
// (deviations.md); everything else is the 640x480 screen alone.
struct ScreenLayout
{
    int width = 0;
    int height = 0;
    int screenX = 0;
    int screenY = 0;
};

// The largest rectangle with the aspect ratio of `layout` (of the 640x480 screen without one) that fits the output,
// centred; the rest stays black.
Viewport FitViewport(int outputWidth, int outputHeight, const ScreenLayout& layout = {});

// An output position mapped to the coordinates of the game's screen on `layout`; positions off the canvas clamp to
// its edge.
LogicalPoint ToLogical(const Viewport& viewport, float outputX, float outputY, const ScreenLayout& layout = {});

// The game window. The original ran full screen at 640x480; here the logical screen keeps that size while the
// window can be resized freely: the screen is scaled with nearest sampling at any ratio that keeps its aspect, with
// black bars on the remaining sides. Alt+Enter toggles borderless full screen. The map can fill it instead
// (SetLayout).
class Display
{
public:
    static constexpr int Width = 640;
    static constexpr int Height = 480;
    static constexpr int Pixels = Width * Height;
    static constexpr int DefaultWindowWidth = 1280;
    static constexpr int DefaultWindowHeight = 960;

    Display() = default;
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    // The window of `width` x `height` (maximised with `maximized`).
    bool Open(
        const std::string& title,
        int width = DefaultWindowWidth,
        int height = DefaultWindowHeight,
        bool maximized = false
    );

    bool IsOpen() const
    {
        return this->renderer != nullptr;
    }

    // The screen shown scaled to the window; a screen larger than the logical one (the map drawn at a render scale)
    // goes through a texture of its own size.
    void Present(const Surface& screen);
    // What the frames shown from now on are (the 640x480 screen alone by default).
    void SetLayout(const ScreenLayout& layout);
    // A picture of the whole canvas shown behind the 640x480 frames presented on a larger layout (nothing behind them
    // without one: black); taken once here.
    void SetBackdrop(const Surface* canvas);
    const ScreenLayout& Layout() const;
    // The canvas the map would fill the window with: as high as the screen and as wide as the window's shape asks
    // (as wide and taller in a tall window), at least the 640x480 screen.
    ScreenLayout FillLayout() const;
    // The largest whole multiple of the canvas the window's output holds (at least 1).
    int FitScale() const;
    // Presenting waits for the display's refresh (`enabled`); false when the renderer cannot.
    bool SetVSync(bool enabled);
    // Handles Alt+Enter; true when the event was consumed.
    bool HandleEvent(const SDL_Event& event);
    void ToggleFullscreen();
    // A window position (as reported by mouse events) in logical screen coordinates.
    LogicalPoint WindowToLogical(float windowX, float windowY) const;
    void WarpPointer(int x, int y);
    // Where a text being typed lies on the logical screen, so that an input method shows its candidates beside it.
    void SetTextInputArea(int x, int y, int width, int height);
    // The icon of the window, and the pointer shown over it while the system cursor is visible (swd32.exe 0x00409EC0
    // registers its window class with icon 0x65 and the blank cursor 0x67).
    void SetIcon(const IconImage& icon);
    void SetCursor(const IconImage& cursor);

    // The rebuild's map overview (deviations.md): pictures of any size drawn on the window's output at any place and
    // size, a frame of them begun cleared to black and shown at its end. A picture made `smooth` is scaled with linear
    // filtering (nearest otherwise); with `keyed`, its pixels of colour `key` are left clear.
    SDL_Texture* CreateOverlay(const Surface& pixels, bool smooth, bool keyed = false, u16 key = 0) const;
    static void DestroyOverlay(SDL_Texture*& overlay);
    static void SetOverlaySmooth(SDL_Texture* overlay, bool smooth);
    void BeginOverlays() const;
    void DrawOverlay(SDL_Texture* overlay, const Viewport& to) const;
    // Black at `alpha` (0..255) over `to`.
    void ShadeOverlay(const Viewport& to, u8 alpha) const;
    void EndOverlays() const;
    // The size of the render output in pixels, where the overlays are placed.
    Point OutputSize() const;

    // The rebuild's smooth map (deviations.md): layers of the map laid by the GPU between game frames. A layer holds
    // RGB565 pixels; it is drawn at a place and size in canvas pixels of the layout (fractions allowed) with nearest
    // sampling.
    enum class LayerBlend
    {
        Add,       // added to what is under it, saturating
        Over,      // over what is under it, its pixels of the key clear
        Subtract,  // each pixel takes from what is under it what it lacks of white (a shadow drawn on white), the key
                   // (white) taking nothing
        Opaque,    // every pixel shown as it is (kept as RGB565, no key)
    };

    class Layer
    {
    public:
        Layer() = default;
        ~Layer();
        Layer(const Layer&) = delete;
        Layer& operator=(const Layer&) = delete;
        Layer(Layer&& other) noexcept;
        Layer& operator=(Layer&& other) noexcept;

        int Width() const
        {
            return this->width;
        }

        int Height() const
        {
            return this->height;
        }

    private:
        friend class Display;

        SDL_Texture* texture = nullptr;
        int width = 0;  // the pixels it holds
        int height = 0;
        int textureWidth = 0;  // the texture's (the largest it held)
        int textureHeight = 0;
        bool opaque = false;  // the texture holds RGB565 (LayerBlend::Opaque), ARGB8888 otherwise
    };

    // `layer` made to hold the `width` x `height` pixels at `pixels` (rows `pitch` pixels apart) for `blend`.
    void UpdateLayer(
        Layer& layer,
        const Pixel* pixels,
        int width,
        int height,
        int pitch,
        Pixel key,
        LayerBlend blend
    ) const;
    // `layer` made to hold what was drawn twice over black (`overBlack`) and over white (`overWhite`): a pixel alike on
    // both is solid, one left black and white clear, one between them translucent (its level and colour taken from
    // the two, as a blend at a level shows them); drawn with LayerBlend::Over.
    void UpdateMattedLayer(
        Layer& layer,
        const Pixel* overBlack,
        const Pixel* overWhite,
        int width,
        int height,
        int pitch
    ) const;
    // A frame of layers begun cleared to black, the layers drawn in order, then shown.
    void BeginLayers() const;
    void DrawLayer(const Layer& layer, float x, float y, float width, float height, LayerBlend blend) const;

    // A layer drawn mirrored, flipped, at an alpha and with its colours scaled (0..255 each).
    struct LayerLook
    {
        bool mirrored = false;
        bool flipped = false;
        u8 alpha = 0xFF;
        u8 red = 0xFF;
        u8 green = 0xFF;
        u8 blue = 0xFF;
    };

    void DrawLayer(
        const Layer& layer,
        float x,
        float y,
        float width,
        float height,
        LayerBlend blend,
        const LayerLook& look
    ) const;

    // A piece of the screen copied within it (canvas pixels): what lies at (sourceX, sourceY) drawn at (x, y).
    struct MapPiece
    {
        float sourceX = 0.0f;
        float sourceY = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // The layers drawn from now on go to a copy of the screen, which EndMapTarget then shows with the pieces copied
    // within it (taken from the copy as it was drawn).
    void BeginMapTarget();
    void EndMapTarget(std::span<const MapPiece> pieces);
    // The layers drawn from now on are clipped to the area (canvas pixels); none: not clipped.
    void ClipLayers(int x, int y, int width, int height) const;
    void UnclipLayers() const;
    // What is drawn on the area (canvas pixels) tinted as Painter::TintPixels tints: each channel raised or lowered
    // by `red`, `green`, `blue` steps of 1/31, saturating.
    void TintArea(float x, float y, float width, float height, int red, int green, int blue) const;
    void EndLayers();
    // While capturing, the layers go to a copy of the output, read back at EndLayers (nothing shown); Captured holds
    // it as RGB565, the output's size.
    void SetCapturing(bool capturing);
    bool Captured(std::vector<Pixel>& pixels, int& width, int& height) const;
    // The map behind a menu as the GPU showed it: `shown` inside the 640x480 screen, `around` around it (the output's
    // size, RGB565); `reference` the 640x480 frame the menu draws on there. A menu frame then shows the map where it
    // still holds the reference, itself elsewhere. Cleared when the menu leaves the map (or the output's size changes).
    void SetMenuBehind(
        std::span<const Pixel> shown,
        std::span<const Pixel> around,
        int width,
        int height,
        std::span<const Pixel> reference
    );
    void ClearMenuBehind();
    // With vsync: the time between the display's refreshes, and when the last frame was shown (SDL_GetTicksNS);
    // 0 without vsync.
    u64 RefreshPeriod() const;
    u64 LastShown() const;
    // The positions of a mouse event in output pixels.
    void ToOutput(SDL_Event& event) const;
    // The system's pointer shown over the window (its arrow, or its move pointer while `moving`), instead of the
    // game's own; `shown` false puts the window's cursor back, hidden as the game keeps it.
    void ShowSystemPointer(bool shown, bool moving = false);

    SDL_Window* Window() const
    {
        return this->window;
    }

    // The window as the user left it: its size when last neither maximised nor full screen, and whether it is
    // maximised (full screen counts as the state before it). `opened` is false when no window was made.
    struct WindowState
    {
        int width = 0;
        int height = 0;
        bool maximized = false;
        bool opened = false;
    };

    WindowState State() const;

private:
    Viewport CurrentViewport() const;
    SDL_Rect CanvasClip() const;
    void TrackWindow(const SDL_Event& event);
    // A logical screen point in window coordinates (what SDL_WarpMouseInWindow and SDL_SetTextInputArea take).
    void LogicalToWindow(int x, int y, float& windowX, float& windowY) const;

    int restoredWidth = DefaultWindowWidth;
    int restoredHeight = DefaultWindowHeight;
    bool maximized = false;
    ScreenLayout layout{Width, Height, 0, 0};
    std::array<int, 4> textInputArea{};  // SetTextInputArea: the area last given to SDL (x, y, width, height)
    SDL_Texture* mapTarget = nullptr;    // BeginMapTarget: the copy of the screen (the output's size)
    bool vsync = false;                  // SetVSync
    mutable bool capturing = false;      // SetCapturing
    mutable SDL_Texture* captureTarget = nullptr;
    mutable std::array<int, 2> captureSize{};
    std::vector<Pixel> captured;  // EndLayers while capturing
    std::array<int, 2> capturedSize{};
    SDL_Texture* menuShown = nullptr;  // SetMenuBehind
    SDL_Texture* menuAround = nullptr;
    SDL_Texture* menuOverlay = nullptr;
    std::array<int, 2> menuSize{};
    std::vector<Pixel> menuReference;
    std::vector<u32> menuOverlayPixels;
    u64 lastShownNs = 0;  // EndLayers
    std::array<int, 2> mapTargetSize{};
    std::vector<float> piecePlaces;   // EndMapTarget: x, y of each corner
    std::vector<float> pieceSources;  // u, v of each corner
    std::vector<int> pieceIndices;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Texture* scaledTexture = nullptr;  // for screens larger than Width x Height
    int scaledWidth = 0;
    int scaledHeight = 0;
    SDL_Texture* backdrop = nullptr;  // SetBackdrop
    int backdropWidth = 0;
    int backdropHeight = 0;
    bool backdropShown = false;
    SDL_Cursor* cursor = nullptr;
    std::array<SDL_Cursor*, 2> systemCursors{};  // ShowSystemPointer: the arrow and the move pointer
};

}  // namespace swd3e
