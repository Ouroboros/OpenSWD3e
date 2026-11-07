#include "platform/display.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "core/log.h"
#include "core/types.h"
#include "platform/exe_resources.h"
#include "platform/surface.h"

namespace swd3e
{

namespace
{

constexpr float kHalf = 0.5F;            // the picture is centred in the window
constexpr int kMostCanvasWidth = 2048;   // the widest canvas the map fills a window with
constexpr int kMostCanvasHeight = 1024;  // the painter's row table holds this many rows

// A copy of `icon` as an SDL surface; null when it is empty.
SDL_Surface* MakeIconSurface(const IconImage& icon)
{
    if (icon.width <= 0 || icon.height <= 0 ||
        icon.pixels.size() != static_cast<std::size_t>(icon.width) * static_cast<std::size_t>(icon.height))
    {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateSurface(icon.width, icon.height, SDL_PIXELFORMAT_ARGB8888);

    if (surface == nullptr)
    {
        return nullptr;
    }

    for (int row = 0; row < icon.height; ++row)
    {
        std::memcpy(
            static_cast<u8*>(surface->pixels) + static_cast<std::ptrdiff_t>(row) * surface->pitch,
            icon.pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(icon.width),
            static_cast<std::size_t>(icon.width) * sizeof(u32)
        );
    }

    return surface;
}

}  // namespace

Viewport FitViewport(int outputWidth, int outputHeight, const ScreenLayout& layout)
{
    const int logicalWidth = layout.width > 0 ? layout.width : Display::Width;
    const int logicalHeight = layout.height > 0 ? layout.height : Display::Height;
    const float scaleX = static_cast<float>(outputWidth) / static_cast<float>(logicalWidth);
    const float scaleY = static_cast<float>(outputHeight) / static_cast<float>(logicalHeight);
    const float scale = std::min(scaleX, scaleY);
    const float width = static_cast<float>(logicalWidth) * scale;
    const float height = static_cast<float>(logicalHeight) * scale;
    return {
        (static_cast<float>(outputWidth) - width) * kHalf,
        (static_cast<float>(outputHeight) - height) * kHalf,
        width,
        height
    };
}

LogicalPoint ToLogical(const Viewport& viewport, float outputX, float outputY, const ScreenLayout& layout)
{
    if (viewport.width <= 0 || viewport.height <= 0)
    {
        return {0, 0};
    }

    const int logicalWidth = layout.width > 0 ? layout.width : Display::Width;
    const int logicalHeight = layout.height > 0 ? layout.height : Display::Height;
    const auto x = static_cast<int>((outputX - viewport.x) * static_cast<float>(logicalWidth) / viewport.width);
    const auto y = static_cast<int>((outputY - viewport.y) * static_cast<float>(logicalHeight) / viewport.height);
    return {
        std::clamp(x, 0, logicalWidth - 1) - layout.screenX,
        std::clamp(y, 0, logicalHeight - 1) - layout.screenY,
    };
}

Display::~Display()
{
    if (this->cursor != nullptr)
    {
        SDL_DestroyCursor(this->cursor);
    }

    for (SDL_Cursor* system : this->systemCursors)
    {
        if (system != nullptr)
        {
            SDL_DestroyCursor(system);
        }
    }

    if (this->texture != nullptr)
    {
        SDL_DestroyTexture(this->texture);
    }

    if (this->scaledTexture != nullptr)
    {
        SDL_DestroyTexture(this->scaledTexture);
    }

    if (this->backdrop != nullptr)
    {
        SDL_DestroyTexture(this->backdrop);
    }

    if (this->mapTarget != nullptr)
    {
        SDL_DestroyTexture(this->mapTarget);
    }

    for (SDL_Texture* texture : {this->captureTarget, this->menuAround})
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
        }
    }

    if (this->renderer != nullptr)
    {
        SDL_DestroyRenderer(this->renderer);
    }

    if (this->window != nullptr)
    {
        SDL_DestroyWindow(this->window);
    }
}

bool Display::Open(const std::string& title, int width, int height, bool maximized)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        LogError(u8"video initialisation failed: %s", AsUtf8(SDL_GetError()));
        return false;
    }

    const SDL_WindowFlags flags =
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (maximized ? SDL_WINDOW_MAXIMIZED : SDL_WindowFlags{0});
    const bool created =
        SDL_CreateWindowAndRenderer(title.c_str(), width, height, flags, &this->window, &this->renderer);

    if (!created)
    {
        LogError(u8"window creation failed: %s", AsUtf8(SDL_GetError()));
        return false;
    }

    SDL_SetWindowMinimumSize(this->window, Display::Width, Display::Height);
    this->restoredWidth = width;
    this->restoredHeight = height;
    this->maximized = maximized;
    this->texture = SDL_CreateTexture(
        this->renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        Display::Width,
        Display::Height
    );

    if (this->texture == nullptr)
    {
        LogError(u8"frame texture creation failed: %s", AsUtf8(SDL_GetError()));
        return false;
    }

    SDL_SetTextureScaleMode(this->texture, SDL_SCALEMODE_NEAREST);
    const char* driver = SDL_GetRendererName(this->renderer);
    LogInfo(
        u8"display open window=%dx%d maximized=%d renderer=%s",
        width,
        height,
        maximized ? 1 : 0,
        AsUtf8(driver != nullptr ? driver : "")
    );
    return true;
}

Viewport Display::CurrentViewport() const
{
    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(this->renderer, &width, &height);
    return FitViewport(width, height, this->layout);
}

void Display::SetLayout(const ScreenLayout& layout)
{
    this->layout = layout;
}

const ScreenLayout& Display::Layout() const
{
    return this->layout;
}

ScreenLayout Display::FillLayout() const
{
    ScreenLayout fill{Display::Width, Display::Height, 0, 0};
    int width = 0;
    int height = 0;

    if (!this->IsOpen() || !SDL_GetRenderOutputSize(this->renderer, &width, &height) || width <= 0 || height <= 0)
    {
        return fill;
    }

    // Even margins keep the screen on whole pixels in the middle.
    if (static_cast<i64>(width) * Display::Height > static_cast<i64>(height) * Display::Width)
    {
        const auto wide = static_cast<int>(static_cast<i64>(Display::Height) * width / height);
        fill.width = std::min(wide & ~1, kMostCanvasWidth);
    }
    else
    {
        const auto tall = static_cast<int>(static_cast<i64>(Display::Width) * height / width);
        fill.height = std::min(tall & ~1, kMostCanvasHeight);
    }

    fill.width = std::max(fill.width, Display::Width);
    fill.height = std::max(fill.height, Display::Height);
    fill.screenX = (fill.width - Display::Width) / 2;
    fill.screenY = (fill.height - Display::Height) / 2;
    return fill;
}

void Display::Present(const Surface& screen)
{
    if (!this->IsOpen())
    {
        return;
    }

    SDL_Texture* shown = this->texture;

    if (screen.Width() != Display::Width || screen.Height() != Display::Height)
    {
        if (this->scaledTexture == nullptr || this->scaledWidth != screen.Width() ||
            this->scaledHeight != screen.Height())
        {
            if (this->scaledTexture != nullptr)
            {
                SDL_DestroyTexture(this->scaledTexture);
            }

            this->scaledTexture = SDL_CreateTexture(
                this->renderer,
                SDL_PIXELFORMAT_RGB565,
                SDL_TEXTUREACCESS_STREAMING,
                screen.Width(),
                screen.Height()
            );
            this->scaledWidth = screen.Width();
            this->scaledHeight = screen.Height();

            if (this->scaledTexture == nullptr)
            {
                LogError(u8"scaled frame texture creation failed: %s", AsUtf8(SDL_GetError()));
                return;
            }

            SDL_SetTextureScaleMode(this->scaledTexture, SDL_SCALEMODE_NEAREST);
            LogInfo(u8"scaled frame texture %dx%d", screen.Width(), screen.Height());
        }

        shown = this->scaledTexture;
    }

    SDL_UpdateTexture(shown, nullptr, screen.Pixels(), screen.Width() * static_cast<int>(sizeof(Pixel)));
    SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
    const Viewport viewport = this->CurrentViewport();
    SDL_FRect target{viewport.x, viewport.y, viewport.width, viewport.height};

    // A frame of the 640x480 screen on a larger layout lies on the screen's place, the backdrop around it.
    if (static_cast<i64>(screen.Width()) * this->layout.height !=
        static_cast<i64>(screen.Height()) * this->layout.width)
    {
        int outputWidth = 0;
        int outputHeight = 0;
        SDL_GetRenderOutputSize(this->renderer, &outputWidth, &outputHeight);
        // The map around a menu as the GPU showed it (SetMenuBehind), while the output keeps its size.
        const bool behind = this->menuAround != nullptr && screen.Width() == Display::Width &&
                            screen.Height() == Display::Height &&
                            this->menuSize == std::array<int, 2>{outputWidth, outputHeight};

        if (behind)
        {
            SDL_RenderTexture(this->renderer, this->menuAround, nullptr, nullptr);
        }
        else if (this->backdropShown && this->backdrop != nullptr)
        {
            SDL_RenderTexture(this->renderer, this->backdrop, nullptr, &target);
        }

        const float scaleX = viewport.width / static_cast<float>(this->layout.width);
        const float scaleY = viewport.height / static_cast<float>(this->layout.height);
        target = {
            viewport.x + static_cast<float>(this->layout.screenX) * scaleX,
            viewport.y + static_cast<float>(this->layout.screenY) * scaleY,
            static_cast<float>(Display::Width) * scaleX,
            static_cast<float>(Display::Height) * scaleY,
        };
    }

    SDL_RenderTexture(this->renderer, shown, nullptr, &target);
    SDL_RenderPresent(this->renderer);
}

void Display::SetBackdrop(const Surface* canvas)
{
    this->backdropShown = false;

    if (canvas == nullptr || !this->IsOpen())
    {
        return;
    }

    if (this->backdrop == nullptr || this->backdropWidth != canvas->Width() || this->backdropHeight != canvas->Height())
    {
        if (this->backdrop != nullptr)
        {
            SDL_DestroyTexture(this->backdrop);
        }

        this->backdrop = SDL_CreateTexture(
            this->renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            canvas->Width(),
            canvas->Height()
        );
        this->backdropWidth = canvas->Width();
        this->backdropHeight = canvas->Height();

        if (this->backdrop == nullptr)
        {
            LogError(u8"backdrop texture creation failed: %s", AsUtf8(SDL_GetError()));
            return;
        }

        SDL_SetTextureScaleMode(this->backdrop, SDL_SCALEMODE_NEAREST);
    }

    SDL_UpdateTexture(this->backdrop, nullptr, canvas->Pixels(), canvas->Width() * static_cast<int>(sizeof(Pixel)));
    this->backdropShown = true;
}

SDL_Texture* Display::CreateOverlay(const Surface& pixels, bool smooth, bool keyed, u16 key) const
{
    if (!this->IsOpen() || pixels.Width() <= 0 || pixels.Height() <= 0)
    {
        return nullptr;
    }

    // SDL makes a surface with a colour key into a texture with alpha, the key clear.
    SDL_Surface* source = SDL_CreateSurfaceFrom(
        pixels.Width(),
        pixels.Height(),
        SDL_PIXELFORMAT_RGB565,
        const_cast<u16*>(pixels.Pixels()),
        pixels.Width() * static_cast<int>(sizeof(u16))
    );

    if (source == nullptr)
    {
        LogError(u8"overlay surface creation failed: %s", AsUtf8(SDL_GetError()));
        return nullptr;
    }

    if (keyed)
    {
        SDL_SetSurfaceColorKey(source, true, key);
    }

    SDL_Texture* overlay = SDL_CreateTextureFromSurface(this->renderer, source);
    SDL_DestroySurface(source);

    if (overlay == nullptr)
    {
        LogError(
            u8"overlay texture %dx%d creation failed: %s",
            pixels.Width(),
            pixels.Height(),
            AsUtf8(SDL_GetError())
        );
        return nullptr;
    }

    Display::SetOverlaySmooth(overlay, smooth);
    return overlay;
}

void Display::DestroyOverlay(SDL_Texture*& overlay)
{
    if (overlay != nullptr)
    {
        SDL_DestroyTexture(overlay);
        overlay = nullptr;
    }
}

void Display::SetOverlaySmooth(SDL_Texture* overlay, bool smooth)
{
    if (overlay != nullptr)
    {
        SDL_SetTextureScaleMode(overlay, smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    }
}

void Display::BeginOverlays() const
{
    if (this->IsOpen())
    {
        SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(this->renderer);
    }
}

void Display::DrawOverlay(SDL_Texture* overlay, const Viewport& to) const
{
    if (this->IsOpen() && overlay != nullptr)
    {
        const SDL_FRect target{to.x, to.y, to.width, to.height};
        SDL_RenderTexture(this->renderer, overlay, nullptr, &target);
    }
}

void Display::ShadeOverlay(const Viewport& to, u8 alpha) const
{
    if (this->IsOpen())
    {
        const SDL_FRect target{to.x, to.y, to.width, to.height};
        SDL_SetRenderDrawBlendMode(this->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, alpha);
        SDL_RenderFillRect(this->renderer, &target);
        SDL_SetRenderDrawBlendMode(this->renderer, SDL_BLENDMODE_NONE);
    }
}

void Display::EndOverlays() const
{
    if (this->IsOpen())
    {
        SDL_RenderPresent(this->renderer);
    }
}

Display::Layer::~Layer()
{
    if (this->texture != nullptr)
    {
        SDL_DestroyTexture(this->texture);
    }
}

Display::Layer::Layer(Layer&& other) noexcept
    : texture(std::exchange(other.texture, nullptr)),
      width(std::exchange(other.width, 0)),
      height(std::exchange(other.height, 0)),
      textureWidth(std::exchange(other.textureWidth, 0)),
      textureHeight(std::exchange(other.textureHeight, 0)),
      opaque(std::exchange(other.opaque, false))
{
}

Display::Layer& Display::Layer::operator=(Layer&& other) noexcept
{
    if (this != &other)
    {
        if (this->texture != nullptr)
        {
            SDL_DestroyTexture(this->texture);
        }

        this->texture = std::exchange(other.texture, nullptr);
        this->width = std::exchange(other.width, 0);
        this->height = std::exchange(other.height, 0);
        this->textureWidth = std::exchange(other.textureWidth, 0);
        this->textureHeight = std::exchange(other.textureHeight, 0);
        this->opaque = std::exchange(other.opaque, false);
    }

    return *this;
}

void Display::UpdateLayer(
    Layer& layer,
    const Pixel* pixels,
    int width,
    int height,
    int pitch,
    Pixel key,
    LayerBlend blend
) const
{
    if (!this->IsOpen() || width <= 0 || height <= 0)
    {
        return;
    }

    const bool opaque = blend == LayerBlend::Opaque;

    if (layer.texture == nullptr || layer.textureWidth < width || layer.textureHeight < height ||
        layer.opaque != opaque)
    {
        if (layer.texture != nullptr)
        {
            SDL_DestroyTexture(layer.texture);
        }

        // A texture grows to the largest layer it held, so that layers of changing sizes keep one.
        if (layer.opaque != opaque)
        {
            layer.textureWidth = 0;
            layer.textureHeight = 0;
        }

        layer.opaque = opaque;
        layer.textureWidth = std::max(width, layer.textureWidth);
        layer.textureHeight = std::max(height, layer.textureHeight);
        layer.texture = SDL_CreateTexture(
            this->renderer,
            opaque ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            layer.textureWidth,
            layer.textureHeight
        );

        if (layer.texture == nullptr)
        {
            LogError(u8"layer texture %dx%d creation failed: %s", width, height, AsUtf8(SDL_GetError()));
            layer.textureWidth = 0;
            layer.textureHeight = 0;
            layer.width = 0;
            layer.height = 0;
            return;
        }

        SDL_SetTextureScaleMode(layer.texture, SDL_SCALEMODE_NEAREST);
    }

    layer.width = width;
    layer.height = height;

    const SDL_Rect area{0, 0, width, height};
    void* locked = nullptr;
    int lockedPitch = 0;

    if (!SDL_LockTexture(layer.texture, &area, &locked, &lockedPitch))
    {
        LogError(u8"layer texture lock failed: %s", AsUtf8(SDL_GetError()));
        return;
    }

    if (opaque)
    {
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(
                static_cast<u8*>(locked) + static_cast<std::ptrdiff_t>(y) * lockedPitch,
                pixels + static_cast<std::ptrdiff_t>(y) * pitch,
                static_cast<std::size_t>(width) * sizeof(Pixel)
            );
        }

        SDL_UnlockTexture(layer.texture);
        return;
    }

    // RGB565 to ARGB8888, each channel's top bits repeated below as the GPU widens a 565 texel; a subtracting layer
    // holds what each channel lacks of its top.
    constexpr u32 kOpaque = 0xFF000000u;
    constexpr u32 kClear = 0;
    constexpr u32 kRedTop = 0x1F;
    constexpr u32 kGreenTop = 0x3F;
    constexpr u32 kBlueTop = 0x1F;
    const bool subtract = blend == LayerBlend::Subtract;
    const u32 keyed = subtract ? kOpaque : kClear;

    for (int y = 0; y < height; ++y)
    {
        const Pixel* from = pixels + static_cast<std::ptrdiff_t>(y) * pitch;
        u32* to = reinterpret_cast<u32*>(static_cast<u8*>(locked) + static_cast<std::ptrdiff_t>(y) * lockedPitch);

        for (int x = 0; x < width; ++x)
        {
            const u32 pixel = from[x];

            if (pixel == key)
            {
                to[x] = keyed;
                continue;
            }

            u32 red = (pixel >> 11) & kRedTop;
            u32 green = (pixel >> 5) & kGreenTop;
            u32 blue = pixel & kBlueTop;

            if (subtract)
            {
                red = kRedTop - red;
                green = kGreenTop - green;
                blue = kBlueTop - blue;
            }

            to[x] =
                kOpaque | ((red << 3 | red >> 2) << 16) | ((green << 2 | green >> 4) << 8) | (blue << 3 | blue >> 2);
        }
    }

    SDL_UnlockTexture(layer.texture);
}

void Display::UpdateMattedLayer(
    Layer& layer,
    const Pixel* overBlack,
    const Pixel* overWhite,
    int width,
    int height,
    int pitch
) const
{
    if (!this->IsOpen() || width <= 0 || height <= 0)
    {
        return;
    }

    constexpr Pixel kBlack = 0;

    if (layer.texture == nullptr || layer.textureWidth < width || layer.textureHeight < height || layer.opaque)
    {
        if (layer.texture != nullptr)
        {
            SDL_DestroyTexture(layer.texture);
        }

        if (layer.opaque)
        {
            layer.textureWidth = 0;
            layer.textureHeight = 0;
        }

        layer.textureWidth = std::max(width, layer.textureWidth);
        layer.textureHeight = std::max(height, layer.textureHeight);
        layer.opaque = false;
        layer.texture = SDL_CreateTexture(
            this->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            layer.textureWidth,
            layer.textureHeight
        );

        if (layer.texture == nullptr)
        {
            LogError(u8"layer texture %dx%d creation failed: %s", width, height, AsUtf8(SDL_GetError()));
            layer.textureWidth = 0;
            layer.textureHeight = 0;
            return;
        }

        SDL_SetTextureScaleMode(layer.texture, SDL_SCALEMODE_NEAREST);
    }

    layer.width = width;
    layer.height = height;
    const SDL_Rect area{0, 0, width, height};
    void* locked = nullptr;
    int lockedPitch = 0;

    if (!SDL_LockTexture(layer.texture, &area, &locked, &lockedPitch))
    {
        LogError(u8"layer texture lock failed: %s", AsUtf8(SDL_GetError()));
        return;
    }

    constexpr Pixel kWhite = 0xFFFF;
    constexpr u32 kTop = 0xFF;
    constexpr u32 kGreenTop = 0x3F;

    // A channel of 565 widened to eight bits.
    auto widen = [](u32 value, int bits)
    {
        return value << (8 - bits) | value >> (2 * bits - 8);
    };

    for (int y = 0; y < height; ++y)
    {
        const Pixel* black = overBlack + static_cast<std::ptrdiff_t>(y) * pitch;
        const Pixel* white = overWhite + static_cast<std::ptrdiff_t>(y) * pitch;
        u32* to = reinterpret_cast<u32*>(static_cast<u8*>(locked) + static_cast<std::ptrdiff_t>(y) * lockedPitch);

        for (int x = 0; x < width; ++x)
        {
            const u32 dark = black[x];
            const u32 light = white[x];
            const u32 red = widen(dark >> 11 & 0x1F, 5);
            const u32 green = widen(dark >> 5 & kGreenTop, 6);
            const u32 blue = widen(dark & 0x1F, 5);

            if (dark == light)
            {
                to[x] = kTop << 24 | red << 16 | green << 8 | blue;
                continue;
            }

            if (dark == kBlack && light == kWhite)
            {
                to[x] = 0;
                continue;
            }

            // Over black a pixel at level a shows a * colour, over white a * colour + (1 - a): the green channel (six
            // bits) gives the level, the colour is what black shows of it over a.
            const u32 gap = (light >> 5 & kGreenTop) - std::min(light >> 5 & kGreenTop, dark >> 5 & kGreenTop);
            const u32 alpha = kTop - gap * kTop / kGreenTop;

            if (alpha == 0)
            {
                to[x] = 0;
                continue;
            }

            auto unblend = [alpha, kTop](u32 channel)
            {
                return std::min(channel * kTop / alpha, kTop);
            };

            to[x] = alpha << 24 | unblend(red) << 16 | unblend(green) << 8 | unblend(blue);
        }
    }

    SDL_UnlockTexture(layer.texture);
}

void Display::BeginLayers(int scale) const
{
    if (!this->IsOpen())
    {
        return;
    }

    constexpr int kLayerParts = 2;  // the layers lie on half canvas pixels
    this->composeScale = std::max(scale, 1) % kLayerParts == 0 ? std::max(scale, 1) : kLayerParts * std::max(scale, 1);
    const int width = this->layout.width * this->composeScale;
    const int height = this->layout.height * this->composeScale;

    if (this->composeTarget == nullptr || this->composeSize != std::array<int, 2>{width, height})
    {
        if (this->composeTarget != nullptr)
        {
            SDL_DestroyTexture(this->composeTarget);
        }

        this->composeTarget =
            SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
        this->composeSize = {width, height};

        if (this->composeTarget == nullptr)
        {
            LogError(u8"compose target creation failed: %s", AsUtf8(SDL_GetError()));
            return;
        }

        SDL_SetTextureScaleMode(this->composeTarget, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(this->composeTarget, SDL_BLENDMODE_NONE);
    }

    SDL_SetRenderTarget(this->renderer, this->composeTarget);
    SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
    const SDL_Rect canvas = this->CanvasClip();
    SDL_SetRenderClipRect(this->renderer, &canvas);
}

int Display::ComposeScale() const
{
    return this->composeScale;
}

// The composing copy whole (the layers are cut at the canvas's edges).
SDL_Rect Display::CanvasClip() const
{
    return {0, 0, this->composeSize[0], this->composeSize[1]};
}

void Display::SetCapturing(bool capturing)
{
    this->capturing = capturing;

    if (capturing)
    {
        this->captured.clear();
        this->capturedSize = {};
    }
}

bool Display::Captured(std::vector<Pixel>& pixels, int& width, int& height) const
{
    if (this->captured.empty())
    {
        return false;
    }

    pixels = this->captured;
    width = this->capturedSize[0];
    height = this->capturedSize[1];
    return true;
}

void Display::SetMenuBehind(std::span<const Pixel> around, int width, int height)
{
    this->ClearMenuBehind();

    if (!this->IsOpen() || around.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
    {
        return;
    }

    this->menuAround =
        SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, width, height);

    if (this->menuAround == nullptr)
    {
        LogError(u8"menu backdrop texture failed: %s", AsUtf8(SDL_GetError()));
        return;
    }

    SDL_SetTextureScaleMode(this->menuAround, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(this->menuAround, nullptr, around.data(), width * static_cast<int>(sizeof(Pixel)));
    this->menuSize = {width, height};
}

void Display::ClearMenuBehind()
{
    if (this->menuAround != nullptr)
    {
        SDL_DestroyTexture(this->menuAround);
        this->menuAround = nullptr;
    }

    this->menuSize = {};
}

void Display::DrawLayer(const Layer& layer, float x, float y, float width, float height, LayerBlend blend) const
{
    this->DrawLayer(layer, x, y, width, height, blend, LayerLook{});
}

void Display::DrawLayer(
    const Layer& layer,
    float x,
    float y,
    float width,
    float height,
    LayerBlend blend,
    const LayerLook& look
) const
{
    if (!this->IsOpen() || layer.texture == nullptr || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    // Canvas pixels to pixels of the composing copy.
    const auto scale = static_cast<float>(this->composeScale);
    const SDL_FRect source{0.0f, 0.0f, static_cast<float>(layer.width), static_cast<float>(layer.height)};
    const SDL_FRect target{x * scale, y * scale, width * scale, height * scale};

    // What is under it less the layer, its alpha kept.
    static const SDL_BlendMode kSubtract = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_ADD
    );
    const SDL_BlendMode mode = blend == LayerBlend::Over       ? SDL_BLENDMODE_BLEND
                               : blend == LayerBlend::Subtract ? kSubtract
                               : blend == LayerBlend::Add      ? SDL_BLENDMODE_ADD
                                                               : SDL_BLENDMODE_NONE;

    // A renderer without the blend (SDL's software renderer has no subtraction) leaves the layer out.
    if (!SDL_SetTextureBlendMode(layer.texture, mode))
    {
        static bool logged = false;

        if (!logged)
        {
            LogError(u8"layer blend unsupported: %s", AsUtf8(SDL_GetError()));
            logged = true;
        }

        return;
    }

    SDL_SetTextureAlphaMod(layer.texture, look.alpha);
    SDL_SetTextureColorMod(layer.texture, look.red, look.green, look.blue);
    const auto flip = static_cast<SDL_FlipMode>(
        (look.mirrored ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) | (look.flipped ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE)
    );

    if (flip == SDL_FLIP_NONE)
    {
        SDL_RenderTexture(this->renderer, layer.texture, &source, &target);
    }
    else
    {
        SDL_RenderTextureRotated(this->renderer, layer.texture, &source, &target, 0.0, nullptr, flip);
    }
}

void Display::ClipLayers(int x, int y, int width, int height) const
{
    if (!this->IsOpen())
    {
        return;
    }

    const int scale = this->composeScale;
    const SDL_Rect area{x * scale, y * scale, width * scale, height * scale};

    const SDL_Rect canvas = this->CanvasClip();
    SDL_Rect clip{};

    if (!SDL_GetRectIntersection(&area, &canvas, &clip))
    {
        clip = {canvas.x, canvas.y, 0, 0};
    }

    SDL_SetRenderClipRect(this->renderer, &clip);
}

void Display::BeginMapTarget()
{
    if (!this->IsOpen())
    {
        return;
    }

    const int width = this->composeSize[0];
    const int height = this->composeSize[1];

    if (this->mapTarget == nullptr || this->mapTargetSize[0] != width || this->mapTargetSize[1] != height)
    {
        if (this->mapTarget != nullptr)
        {
            SDL_DestroyTexture(this->mapTarget);
        }

        this->mapTarget =
            SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
        this->mapTargetSize = {width, height};

        if (this->mapTarget == nullptr)
        {
            LogError(u8"map target creation failed: %s", AsUtf8(SDL_GetError()));
            return;
        }

        SDL_SetTextureScaleMode(this->mapTarget, SDL_SCALEMODE_NEAREST);
    }

    SDL_SetRenderTarget(this->renderer, this->mapTarget);
    SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
}

void Display::EndMapTarget(std::span<const MapPiece> pieces)
{
    if (!this->IsOpen() || this->mapTarget == nullptr)
    {
        return;
    }

    // Back to the composing copy.
    SDL_SetRenderTarget(this->renderer, this->composeTarget);
    const SDL_Rect canvas = this->CanvasClip();
    SDL_SetRenderClipRect(this->renderer, &canvas);
    SDL_SetTextureBlendMode(this->mapTarget, SDL_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(this->mapTarget, SDL_ALPHA_OPAQUE);
    SDL_SetTextureColorMod(this->mapTarget, 0xFF, 0xFF, 0xFF);
    SDL_RenderTexture(this->renderer, this->mapTarget, nullptr, nullptr);

    if (pieces.empty())
    {
        return;
    }

    // Canvas pixels to pixels of the composing copy, and to the map copy's texture coordinates.
    const auto scaleX = static_cast<float>(this->composeScale);
    const float scaleY = scaleX;
    const auto textureWidth = static_cast<float>(this->mapTargetSize[0]);
    const auto textureHeight = static_cast<float>(this->mapTargetSize[1]);
    constexpr SDL_FColor kWhole{1.0f, 1.0f, 1.0f, 1.0f};
    constexpr std::array<int, 6> kQuad = {0, 1, 2, 2, 1, 3};
    constexpr int kCoordinates = 2;
    this->piecePlaces.clear();
    this->pieceSources.clear();
    this->pieceIndices.clear();

    for (const MapPiece& piece : pieces)
    {
        const float left = piece.x * scaleX;
        const float top = piece.y * scaleY;
        const float right = left + piece.width * scaleX;
        const float bottom = top + piece.height * scaleY;
        const float sourceLeft = piece.sourceX * scaleX / textureWidth;
        const float sourceTop = piece.sourceY * scaleY / textureHeight;
        const float sourceRight = sourceLeft + piece.width * scaleX / textureWidth;
        const float sourceBottom = sourceTop + piece.height * scaleY / textureHeight;
        const auto first = static_cast<int>(this->piecePlaces.size()) / kCoordinates;
        this->piecePlaces.insert(this->piecePlaces.end(), {left, top, right, top, left, bottom, right, bottom});
        this->pieceSources.insert(
            this->pieceSources.end(),
            {sourceLeft, sourceTop, sourceRight, sourceTop, sourceLeft, sourceBottom, sourceRight, sourceBottom}
        );

        for (const int corner : kQuad)
        {
            this->pieceIndices.push_back(first + corner);
        }
    }

    // One colour for every corner (a stride of 0).
    SDL_RenderGeometryRaw(
        this->renderer,
        this->mapTarget,
        this->piecePlaces.data(),
        static_cast<int>(kCoordinates * sizeof(float)),
        &kWhole,
        0,
        this->pieceSources.data(),
        static_cast<int>(kCoordinates * sizeof(float)),
        static_cast<int>(this->piecePlaces.size()) / kCoordinates,
        this->pieceIndices.data(),
        static_cast<int>(this->pieceIndices.size()),
        sizeof(int)
    );
}

void Display::UnclipLayers() const
{
    if (this->IsOpen())
    {
        const SDL_Rect canvas = this->CanvasClip();
        SDL_SetRenderClipRect(this->renderer, &canvas);
    }
}

void Display::TintArea(float x, float y, float width, float height, int red, int green, int blue) const
{
    if (!this->IsOpen())
    {
        return;
    }

    constexpr int kSteps = 31;
    constexpr int kTop = 0xFF;
    static const SDL_BlendMode kRaise = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_ADD
    );
    static const SDL_BlendMode kLower = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_ADD
    );

    auto amount = [kSteps](int level)
    {
        return static_cast<u8>(std::min(level, kSteps) * kTop / kSteps);
    };

    const auto scale = static_cast<float>(this->composeScale);
    const SDL_FRect target{x * scale, y * scale, width * scale, height * scale};

    if ((red > 0 || green > 0 || blue > 0) && SDL_SetRenderDrawBlendMode(this->renderer, kRaise))
    {
        SDL_SetRenderDrawColor(
            this->renderer,
            amount(std::max(red, 0)),
            amount(std::max(green, 0)),
            amount(std::max(blue, 0)),
            SDL_ALPHA_OPAQUE
        );
        SDL_RenderFillRect(this->renderer, &target);
    }

    if ((red < 0 || green < 0 || blue < 0) && SDL_SetRenderDrawBlendMode(this->renderer, kLower))
    {
        SDL_SetRenderDrawColor(
            this->renderer,
            amount(std::max(-red, 0)),
            amount(std::max(-green, 0)),
            amount(std::max(-blue, 0)),
            SDL_ALPHA_OPAQUE
        );
        SDL_RenderFillRect(this->renderer, &target);
    }

    SDL_SetRenderDrawBlendMode(this->renderer, SDL_BLENDMODE_NONE);
}

void Display::EndLayers()
{
    if (!this->IsOpen())
    {
        return;
    }

    SDL_SetRenderClipRect(this->renderer, nullptr);

    // The composed canvas scaled to the window once (to the output's copy while capturing).
    if (this->capturing)
    {
        int width = 0;
        int height = 0;
        SDL_GetRenderOutputSize(this->renderer, &width, &height);

        if (this->captureTarget == nullptr || this->captureSize != std::array<int, 2>{width, height})
        {
            if (this->captureTarget != nullptr)
            {
                SDL_DestroyTexture(this->captureTarget);
            }

            this->captureTarget =
                SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
            this->captureSize = {width, height};
        }
    }

    SDL_SetRenderTarget(this->renderer, this->capturing ? this->captureTarget : nullptr);
    SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(this->renderer);
    const Viewport viewport = this->CurrentViewport();
    const SDL_FRect shown{viewport.x, viewport.y, viewport.width, viewport.height};
    SDL_RenderTexture(this->renderer, this->composeTarget, nullptr, &shown);

    // Captured: the copy read back as RGB565, nothing shown.
    if (this->capturing)
    {
        SDL_Surface* read = SDL_RenderReadPixels(this->renderer, nullptr);
        SDL_SetRenderTarget(this->renderer, nullptr);

        if (read == nullptr)
        {
            LogError(u8"capture read failed: %s", AsUtf8(SDL_GetError()));
            return;
        }

        SDL_Surface* converted = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGB565);
        SDL_DestroySurface(read);

        if (converted == nullptr)
        {
            return;
        }

        this->captured.resize(static_cast<std::size_t>(converted->w) * static_cast<std::size_t>(converted->h));

        for (int y = 0; y < converted->h; ++y)
        {
            std::memcpy(
                this->captured.data() + static_cast<std::ptrdiff_t>(y) * converted->w,
                static_cast<const u8*>(converted->pixels) + static_cast<std::ptrdiff_t>(y) * converted->pitch,
                sizeof(Pixel) * static_cast<std::size_t>(converted->w)
            );
        }

        this->capturedSize = {converted->w, converted->h};
        SDL_DestroySurface(converted);
        return;
    }

    SDL_RenderPresent(this->renderer);
    this->lastShownNs = SDL_GetTicksNS();
}

u64 Display::RefreshPeriod() const
{
    if (!this->IsOpen() || !this->vsync)
    {
        return 0;
    }

    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(this->window));
    constexpr double kNanosecondsPerSecond = 1e9;
    return mode == nullptr || mode->refresh_rate <= 0.0f
               ? 0
               : static_cast<u64>(kNanosecondsPerSecond / static_cast<double>(mode->refresh_rate));
}

u64 Display::LastShown() const
{
    return this->lastShownNs;
}

Point Display::OutputSize() const
{
    Point size{Display::Width, Display::Height};

    if (this->IsOpen())
    {
        SDL_GetRenderOutputSize(this->renderer, &size.x, &size.y);
    }

    return size;
}

void Display::ToOutput(SDL_Event& event) const
{
    if (this->IsOpen())
    {
        SDL_ConvertEventToRenderCoordinates(this->renderer, &event);
    }
}

void Display::ShowSystemPointer(bool shown, bool moving)
{
    if (!shown)
    {
        if (this->cursor != nullptr)
        {
            SDL_SetCursor(this->cursor);
        }

        SDL_HideCursor();
        return;
    }

    constexpr std::array<SDL_SystemCursor, 2> kKinds = {SDL_SYSTEM_CURSOR_DEFAULT, SDL_SYSTEM_CURSOR_MOVE};
    const std::size_t kind = moving ? 1 : 0;

    if (this->systemCursors[kind] == nullptr)
    {
        this->systemCursors[kind] = SDL_CreateSystemCursor(kKinds[kind]);
    }

    if (this->systemCursors[kind] != nullptr)
    {
        SDL_SetCursor(this->systemCursors[kind]);
    }

    SDL_ShowCursor();
}

int Display::FitScale() const
{
    if (!this->IsOpen())
    {
        return 1;
    }

    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(this->renderer, &width, &height);
    return std::max(1, std::min(width / this->layout.width, height / this->layout.height));
}

bool Display::SetVSync(bool enabled)
{
    this->vsync = false;

    if (!this->IsOpen() || !SDL_SetRenderVSync(this->renderer, enabled ? 1 : 0))
    {
        LogWarning(
            u8"vsync %s failed: %s",
            enabled ? std::u8string_view(u8"on") : std::u8string_view(u8"off"),
            AsUtf8(SDL_GetError())
        );
        return false;
    }

    this->vsync = enabled;
    return true;
}

bool Display::HandleEvent(const SDL_Event& event)
{
    this->TrackWindow(event);
    // The keypad Enter works as Enter (deviations.md).
    const bool altEnter =
        event.type == SDL_EVENT_KEY_DOWN && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER);

    if (altEnter && (event.key.mod & SDL_KMOD_ALT) != 0 && !event.key.repeat)
    {
        this->ToggleFullscreen();
        return true;
    }

    return false;
}

void Display::TrackWindow(const SDL_Event& event)
{
    if (this->window == nullptr)
    {
        return;
    }

    const SDL_WindowFlags flags = SDL_GetWindowFlags(this->window);
    const SDL_WindowFlags changed = SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED;

    if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
    {
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED && (flags & changed) == 0)
    {
        this->restoredWidth = event.window.data1;
        this->restoredHeight = event.window.data2;
    }
    else if (event.type == SDL_EVENT_WINDOW_MAXIMIZED)
    {
        this->maximized = true;
    }
    else if (event.type == SDL_EVENT_WINDOW_RESTORED && (flags & SDL_WINDOW_MINIMIZED) == 0)
    {
        this->maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    }
}

Display::WindowState Display::State() const
{
    WindowState state{this->restoredWidth, this->restoredHeight, this->maximized, this->window != nullptr};

    if (this->window != nullptr && (SDL_GetWindowFlags(this->window) & SDL_WINDOW_FULLSCREEN) == 0)
    {
        state.maximized = (SDL_GetWindowFlags(this->window) & SDL_WINDOW_MAXIMIZED) != 0;
    }

    return state;
}

void Display::ToggleFullscreen()
{
    if (this->window == nullptr)
    {
        return;
    }

    const bool fullscreen = (SDL_GetWindowFlags(this->window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreenMode(this->window, nullptr);
    SDL_SetWindowFullscreen(this->window, !fullscreen);
}

LogicalPoint Display::WindowToLogical(float windowX, float windowY) const
{
    if (!this->IsOpen())
    {
        return {0, 0};
    }

    int windowWidth = 0;
    int windowHeight = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
    SDL_GetRenderOutputSize(this->renderer, &outputWidth, &outputHeight);
    const float scaleX = windowWidth > 0 ? static_cast<float>(outputWidth) / windowWidth : 1.0f;
    const float scaleY = windowHeight > 0 ? static_cast<float>(outputHeight) / windowHeight : 1.0f;
    return ToLogical(this->CurrentViewport(), windowX * scaleX, windowY * scaleY, this->layout);
}

// SetCursorPos(x, y): the system pointer moved to logical point (x, y) of the window (the original runs full screen,
// where the screen point is the logical one)
void Display::WarpPointer(int x, int y)
{
    if (!this->IsOpen())
    {
        return;
    }

    float windowX = 0;
    float windowY = 0;
    this->LogicalToWindow(x, y, windowX, windowY);
    SDL_WarpMouseInWindow(this->window, windowX, windowY);
}

void Display::LogicalToWindow(int x, int y, float& windowX, float& windowY) const
{
    int windowWidth = 0;
    int windowHeight = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
    SDL_GetRenderOutputSize(this->renderer, &outputWidth, &outputHeight);
    const float scaleX = outputWidth > 0 ? static_cast<float>(windowWidth) / outputWidth : 1.0f;
    const float scaleY = outputHeight > 0 ? static_cast<float>(windowHeight) / outputHeight : 1.0f;
    const Viewport viewport = this->CurrentViewport();
    const float outputX = viewport.x + static_cast<float>(x + this->layout.screenX) * viewport.width /
                                           static_cast<float>(this->layout.width);
    const float outputY = viewport.y + static_cast<float>(y + this->layout.screenY) * viewport.height /
                                           static_cast<float>(this->layout.height);
    windowX = outputX * scaleX;
    windowY = outputY * scaleY;
}

// The rebuild: the area is given again only when it moved (the window resized or the box elsewhere).
void Display::SetTextInputArea(int x, int y, int width, int height)
{
    if (!this->IsOpen())
    {
        return;
    }

    float left = 0;
    float top = 0;
    float right = 0;
    float bottom = 0;
    this->LogicalToWindow(x, y, left, top);
    this->LogicalToWindow(x + width, y + height, right, bottom);
    const SDL_Rect area{
        static_cast<int>(std::round(left)),
        static_cast<int>(std::round(top)),
        static_cast<int>(std::round(right - left)),
        static_cast<int>(std::round(bottom - top)),
    };

    const std::array<int, 4> given{area.x, area.y, area.w, area.h};

    if (given == this->textInputArea)
    {
        return;
    }

    this->textInputArea = given;
    SDL_SetTextInputArea(this->window, &area, 0);
}

void Display::SetIcon(const IconImage& icon)
{
    SDL_Surface* surface = MakeIconSurface(icon);

    if (this->window != nullptr && surface != nullptr && !SDL_SetWindowIcon(this->window, surface))
    {
        LogError(u8"window icon not set: %s", AsUtf8(SDL_GetError()));
    }

    SDL_DestroySurface(surface);
}

void Display::SetCursor(const IconImage& cursor)
{
    SDL_Surface* surface = MakeIconSurface(cursor);

    if (surface == nullptr)
    {
        return;
    }

    SDL_Cursor* made = SDL_CreateColorCursor(surface, cursor.hotX, cursor.hotY);
    SDL_DestroySurface(surface);

    if (made == nullptr)
    {
        LogError(u8"cursor not made: %s", AsUtf8(SDL_GetError()));
        return;
    }

    SDL_SetCursor(made);

    if (this->cursor != nullptr)
    {
        SDL_DestroyCursor(this->cursor);
    }

    this->cursor = made;
}

}  // namespace swd3e
