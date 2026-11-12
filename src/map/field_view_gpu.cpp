#include "map/field_view_internal.h"

namespace swd3e
{

namespace
{

// The recording samples the zoomed pictures at twice the original's resolution: 2 x 2 blocks woven together, each
// painted as a block of the render scale 2 (PaintBetween).
constexpr int kRecordParts = 2;
constexpr std::size_t kBlocks = 4;
// The recorded canvas reaches this far past each side of the screen, so that the camera gliding through a game
// frame finds the map recorded.
constexpr i32 kRecordMargin = 32;
// The recorded canvas's corner lies on a grid of the map this wide (the canvas a cell wider), so that the layers stay
// put while the camera moves within a cell.
constexpr i32 kRecordGrid = 64;
// A layer past the first is drawn over black and over white (what it blends then keeps its level, Display::
// UpdateMattedLayer); its shadows over white (a shadow takes away what it lacks of white).
constexpr Pixel kBlackPixel = 0x0000;
constexpr Pixel kWhitePixel = 0xFFFF;
// The top byte of an effect element's key holds its kind (EffectKey).
constexpr int kEffectKindShift = 56;

// Diagnostics (smooth gpu): the time of the recordings, the interface and presenting, and their counts.
[[maybe_unused]] std::array<u64, 5> gpuTimes = {};
[[maybe_unused]] std::array<u64, 4> gpuCounts = {};
// Diagnostics (smooth gpu): a recording's steps (first block, other blocks, white, shade, hand-over).
[[maybe_unused]] std::array<u64, 5> recordSteps = {};
[[maybe_unused]] u64 recordStepCount = 0;
[[maybe_unused]] u64 recordLayersChanged = 0;
[[maybe_unused]] u64 recordPixelsHanded = 0;
[[maybe_unused]] u64 recordUploadNs = 0;
// Diagnostics (smooth gpu): the times between frames shown (under 5, 9, 13, 17 ms and longer) and the slow ones.
[[maybe_unused]] std::array<u64, 5> intervalCounts = {};
[[maybe_unused]] u64 lastShownNs = 0;
[[maybe_unused]] u32 lastShownGameFrame = 0;
[[maybe_unused]] u64 intervalMostNs = 0;

void FillRect(Surface& surface, i32 left, i32 top, i32 right, i32 bottom, Pixel value)
{
    left = std::max(left, 0);
    top = std::max(top, 0);
    right = std::min(right, surface.Width());
    bottom = std::min(bottom, surface.Height());

    for (i32 y = top; y < bottom; ++y)
    {
        std::fill(surface.Row(y) + left, surface.Row(y) + right, value);
    }
}

// `surface` made `width` x `height`, every pixel `value` when it was made anew.
// What `block` holds within `dirty`, row after row.
void CopyBlock(const Surface& block, const std::array<i32, 4>& dirty, std::vector<Pixel>& out)
{
    out.clear();

    for (i32 y = dirty[1]; y < dirty[3]; ++y)
    {
        const Pixel* row = block.Row(y);
        out.insert(out.end(), row + dirty[0], row + dirty[2]);
    }
}

void Prepare(Surface& surface, i32 width, i32 height, Pixel value)
{
    if (surface.Width() != width || surface.Height() != height)
    {
        surface.Resize(width, height);
        FillRect(surface, 0, 0, width, height, value);
    }
}

}  // namespace

// What the recording drew on the layer and block it draws on, in pixels of the layer's canvas.
void FieldView::RecordDirty(i32 left, i32 top, i32 right, i32 bottom)
{
    GpuLayer& layer = this->gpuLayers[this->recordLayer];
    std::array<i32, 4>& dirty = layer.dirty;
    left = std::max(left, 0);
    top = std::max(top, 0);
    right = std::min(right, layer.canvasWidth);
    bottom = std::min(bottom, layer.canvasHeight);

    if (left >= right || top >= bottom)
    {
        return;
    }

    if (dirty[0] >= dirty[2] || dirty[1] >= dirty[3])
    {
        dirty = {left, top, right, bottom};
        return;
    }

    dirty = {std::min(dirty[0], left), std::min(dirty[1], top), std::max(dirty[2], right), std::max(dirty[3], bottom)};
}

// What the first block of `layer` holds besides black, two pixels wider each way (the other blocks sample the zoomed
// pictures half a pixel apart, and what is drawn black lies beside what is not), taken as what the layer's blocks
// hold.
void FieldView::ScanDirty(GpuLayer& layer)
{
    const Surface& block = layer.colors[0];
    constexpr i32 kBlockSpread = 2;
    i32 left = layer.canvasWidth;
    i32 top = layer.canvasHeight;
    i32 right = 0;
    i32 bottom = 0;

    for (i32 y = 0; y < block.Height(); ++y)
    {
        const Pixel* row = block.Row(y);
        const Pixel* end = row + block.Width();
        const Pixel* first = std::find_if(
            row,
            end,
            [](Pixel pixel)
            {
                return pixel != kBlackPixel;
            }
        );

        if (first == end)
        {
            continue;
        }

        const auto last = std::find_if(
            std::make_reverse_iterator(end),
            std::make_reverse_iterator(first),
            [](Pixel pixel)
            {
                return pixel != kBlackPixel;
            }
        );
        left = std::min(left, static_cast<i32>(first - row));
        right = std::max(right, static_cast<i32>(last.base() - row));
        top = std::min(top, y);
        bottom = y + 1;
    }

    layer.dirty = {};
    const std::size_t kept = this->recordLayer;
    this->recordLayer = static_cast<std::size_t>(&layer - this->gpuLayers.data());
    this->RecordDirty(left - kBlockSpread, top - kBlockSpread, right + kBlockSpread, bottom + kBlockSpread);
    this->recordLayer = kept;
}

// The painter and the screen set to draw on the recording's layer and block: its colour canvas, or its shade canvas in
// the shade pass (layers past the first only).
void FieldView::RecordTarget()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    GpuLayer& layer = this->gpuLayers[this->recordLayer];
    const RecordPass pass = this->recordPass;
    // The first layer's canvas in the white and shade passes: what is drawn there is not kept.
    // (Over white kept for the first layer too when the overlays lie under it, recordOverlays.)
    const bool keptWhite = pass == RecordPass::White && (this->recordLayer > 0 || this->recordOverlays);
    // (The first layer, the ground, is recorded by its first block alone.)
    // (A layer at the canvas's resolution is recorded by its first block alone; an added one needs no white.)
    this->recordSkip =
        (pass != RecordPass::Color || this->recordBlock != 0) &&
        (!layer.changed || (layer.single && this->recordBlock != 0) || (layer.additive && pass == RecordPass::White));
    Surface& canvas = this->recordSkip            ? this->recordSkipScratch
                      : pass == RecordPass::Color ? layer.colors[this->recordBlock]
                      : keptWhite                 ? layer.whites[this->recordBlock]
                      : this->recordLayer == 0    ? this->recordScratch
                                                  : layer.shades[this->recordBlock];

    // What a skipped layer draws is of no use: its canvas is any as large (made larger alone).
    if (this->recordSkip)
    {
        const i64 needed = static_cast<i64>(layer.canvasWidth) * layer.canvasHeight;

        if (static_cast<i64>(canvas.Width()) * canvas.Height() < needed)
        {
            canvas.Resize(std::max(canvas.Width(), layer.canvasWidth), std::max(canvas.Height(), layer.canvasHeight));
        }
    }
    else
    {
        Prepare(canvas, layer.canvasWidth, layer.canvasHeight, pass == RecordPass::Color ? kBlackPixel : kWhitePixel);
    }

    owner.SetDrawSize(layer.canvasWidth, layer.canvasHeight);
    owner.drawSurface = canvas.Pixels();
    painter.SetTarget(owner.drawSurface);
    painter.SetClipClamped(0, 0, layer.canvasWidth, layer.canvasHeight);
    const Scene& view = owner.scene;
    const GameState& state = owner.state;

    // A block begins with the spotlight where it glides now (PaintKept put the game frame's place back).
    if (this->recordLayer == 0 && this->gpuSpotlightMoving)
    {
        owner.scene.spotlightX = this->gpuSpotlight[0];
        owner.scene.spotlightY = this->gpuSpotlight[1];
    }

    // The clip to the spotlight PaintWorld set once its overlays were drawn, or the frame from its start, kept on the
    // still layers after a sprite (a sprite is clipped by the GPU, where it glides).
    const bool spotlit = !state.GameFlag(GameState::PlainScreen) && state.GameFlag(GameState::Spotlight);

    // The rebuild's map filling the window (deviations.md): a still layer (the screen's place) kept on the map, as
    // PaintWorld keeps it.
    if (layer.role >= GameState::Roles)
    {
        this->ClipToMap();
    }

    if (layer.role >= GameState::Roles &&
        ((view.screenFlags & Scene::SpotlightClip) != 0 || (spotlit && this->recordLayer > 0)))
    {
        this->ClipToSpotlight();
    }
}

// A moving role met by DrawRoles while recording: drawn on its sprite (the camera set to put the role at the sprite's
// origin), the roles after it then drawn on the still layer after the sprite. False for a role that is not moving.
bool FieldView::RecordSprite(Role& role)
{
    const std::size_t index = this->RoleIndex(role);

    if (index >= GameState::Roles || this->recordSpriteOf[index] == 0)
    {
        return false;
    }

    Game& owner = this->game;
    Field& field = owner.field;

    // The first block of the colour pass meets the moving roles in drawing order: each takes the next sprite, the still
    // layer after it the next.
    if (this->recordSpriteOf[index] < 0)
    {
        // The picture DrawRole draws (a role without an act takes its picture without a frame).
        const Picture* picture =
            owner.archive.GetPicture(role.act.id != 0 ? role.act.frame : kNoFrame, role.act.picture);

        if (picture == nullptr)
        {
            this->recordFailed = true;
            return true;
        }

        const std::size_t sprite = this->recordLayer + 1;
        this->ClaimLayer(sprite, index);

        GpuLayer& made = this->gpuLayers[sprite];
        made.role = index;
        // What is not zoomed (DrawRole: a role without an id or with NoDepthScale) is drawn alike on every block.
        made.single = role.id == Role::NoId || (role.flags & Role::NoDepthScale) != 0;
        made.additive = false;
        made.originX = role.act.x - role.drawOffsetX + kLayerRoleSide;
        made.originY = role.act.y - role.drawOffsetY + kLayerRoleTop;
        made.canvasWidth = picture->width + 2 * kLayerRoleSide;
        made.spriteHeight = picture->height + kLayerRoleTop + kLayerRoleBottom;
        const bool labelled = owner.state.leader.id == Role::NoId && role.label.has_value();

        // A label (DrawRole) reaches past the picture: the sprite takes it whole, its shadow too.
        if (labelled)
        {
            constexpr i32 kShadowReach = 2;
            const TextFont& font = owner.largeFont;
            const auto length = static_cast<i32>(role.label->size());
            const i32 left = kLabelShift - kLabelGlyphWidth * length / 2;
            const i32 right = left + font.Advance() * length / 2 + font.GlyphWidth() + kShadowReach;
            const i32 top = made.originY - role.act.y;
            const i32 widen = std::max(0, -(made.originX + left));
            const i32 raise = std::max(0, -top);
            made.originX += widen;
            made.canvasWidth = std::max(made.canvasWidth + widen, made.originX + right);
            made.originY += raise;
            made.spriteHeight = std::max(made.spriteHeight + raise, top + raise + font.GlyphHeight() + kShadowReach);
        }

        // Diagnostics (smooth gpu): the sprites of the roles with an attached act.
        if constexpr (BuildOptions::Diagnostics)
        {
            if (role.attached != nullptr)
            {
                static int attachedLogs = 0;
                const Picture* extra = owner.archive.GetPicture(role.attached->frame, role.attached->picture);

                if (attachedLogs < 40)
                {
                    ++attachedLogs;
                    LogDiagnostic(
                        u8"attached sprite: role %u act %d,%d picture %dx%d at %d,%d; sprite origin %d,%d canvas %dx%d",
                        static_cast<u32>(index),
                        role.attached->id,
                        static_cast<i32>(role.attached->frame),
                        extra != nullptr ? static_cast<i32>(extra->width) : -1,
                        extra != nullptr ? static_cast<i32>(extra->height) : -1,
                        role.drawOffsetX - role.attached->x,
                        role.drawOffsetY - role.act.y,
                        made.originX,
                        made.originY,
                        made.canvasWidth,
                        made.spriteHeight
                    );
                }
            }
        }

        // Diagnostics (smooth gpu): the sprites of the roles with a label.
        if constexpr (BuildOptions::Diagnostics)
        {
            if (role.label.has_value())
            {
                static int labelLogs = 0;

                if (labelLogs < 40)
                {
                    ++labelLogs;
                    LogDiagnostic(
                        u8"label sprite: role %u leader %d labelled %d bytes %u origin %d canvas %d font %d,%d",
                        static_cast<u32>(index),
                        static_cast<i32>(owner.state.leader.id),
                        labelled ? 1 : 0,
                        static_cast<u32>(role.label->size()),
                        made.originX,
                        made.canvasWidth,
                        owner.largeFont.Advance(),
                        owner.largeFont.GlyphWidth()
                    );
                }
            }
        }

        // The font writes on rows of the screen's height (a role's label).
        made.canvasHeight = labelled ? std::max(made.spriteHeight, Display::Height) : made.spriteHeight;
        this->recordSpriteOf[index] = static_cast<i32>(sprite);
    }

    const auto sprite = static_cast<std::size_t>(this->recordSpriteOf[index]);
    GpuLayer& layer = this->gpuLayers[sprite];
    const std::array<i32, 4> camera = {field.cameraLeft, field.cameraTop, field.cameraRight, field.cameraBottom};
    const i32 paintCameraX = this->paintCameraX;
    const i32 paintCameraY = this->paintCameraY;
    field.cameraLeft = role.x - layer.originX;
    field.cameraTop = role.y - layer.originY;
    field.cameraRight = field.cameraLeft + layer.canvasWidth;
    field.cameraBottom = field.cameraTop + layer.canvasHeight;
    this->paintCameraX = kRecordParts * field.cameraLeft;
    this->paintCameraY = kRecordParts * field.cameraTop;
    this->recordLayer = sprite;
    this->RecordTarget();
    this->RecordDirty(0, 0, layer.canvasWidth, layer.spriteHeight);
    this->recordInSprite = true;
    this->DrawRole(role);
    this->recordInSprite = false;
    field.cameraLeft = camera[0];
    field.cameraTop = camera[1];
    field.cameraRight = camera[2];
    field.cameraBottom = camera[3];
    this->paintCameraX = paintCameraX;
    this->paintCameraY = paintCameraY;
    // What is drawn next goes to a still layer after the sprite, made when something is (RecordStill); a sprite next
    // follows this one.
    this->recordLayer = sprite;
    this->recordStillPending = true;
    return true;
}

// The layer at `at` made the one of `role` (Roles: a still layer) as the first block meets it: the layer that held
// it before brought there, else a new one put in, so that what the layers past it hold (the roles drawn later, which
// shift as roles come into and leave the canvas) stays with their roles.
void FieldView::ClaimLayer(std::size_t at, std::size_t role)
{
    std::vector<GpuLayer>& layers = this->gpuLayers;

    if (layers.size() < at + 1)
    {
        layers.resize(at + 1);
    }

    if (layers[at].keptRole == role)
    {
        return;
    }

    for (std::size_t index = at + 1; index < layers.size(); ++index)
    {
        if (layers[index].keptRole == role)
        {
            const auto first = layers.begin() + static_cast<std::ptrdiff_t>(at);
            const auto found = layers.begin() + static_cast<std::ptrdiff_t>(index);
            std::rotate(first, found, found + 1);
            return;
        }
    }

    layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(at), GpuLayer{});
}

// The still layer after the sprite last recorded, when something is drawn on it: made by the first block of the
// colour pass (the size of the first layer), taken again by the other passes.
void FieldView::RecordStill()
{
    if (!this->recordStillPending)
    {
        return;
    }

    this->recordStillPending = false;
    const std::size_t still = this->recordLayer + 1;

    if (this->recordPass == RecordPass::Color && this->recordBlock == 0)
    {
        this->ClaimLayer(still, GameState::Roles);
        GpuLayer& next = this->gpuLayers[still];
        next.role = GameState::Roles;
        next.single = false;
        next.additive = false;
        next.canvasWidth = this->gpuLayers[0].canvasWidth;
        next.canvasHeight = this->gpuLayers[0].canvasHeight;
    }

    this->recordLayer = still;
    this->RecordTarget();
}

// What `blocks` hold within `dirty` woven into `woven` at twice the resolution.
void FieldView::WeaveBlocks(
    const std::array<Surface, 4>& blocks,
    const std::array<i32, 4>& dirty,
    std::vector<Pixel>& woven
)
{
    const i32 width = dirty[2] - dirty[0];
    const i32 height = dirty[3] - dirty[1];
    const i32 wovenWidth = kRecordParts * width;
    woven.resize(static_cast<std::size_t>(wovenWidth) * static_cast<std::size_t>(kRecordParts * height));

    for (std::size_t block = 0; block < kBlocks; ++block)
    {
        const auto column = static_cast<i32>(block % kRecordParts);
        const auto row = static_cast<i32>(block / kRecordParts);

        for (i32 y = 0; y < height; ++y)
        {
            const Pixel* from = blocks[block].Row(dirty[1] + y) + dirty[0];
            Pixel* to = woven.data() + static_cast<std::ptrdiff_t>(kRecordParts * y + row) * wovenWidth + column;

            for (i32 x = 0; x < width; ++x)
            {
                to[kRecordParts * x] = from[x];
            }
        }
    }
}

// The map of the game frame recorded in layers (once a frame): the still parts and each moving role apart, sampled at
// twice the resolution. The first block is painted first; a layer whose first block holds what it held when it was
// last handed to the GPU is kept, and the other blocks are painted only for those that changed. False when the frame
// drew what a layer cannot hold.
bool FieldView::RecordLayers()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Field& field = owner.field;
    Scene& view = owner.scene;
    // The recorded canvas lies on a grid of the map, so that it stays put while the camera moves within a cell.
    const i32 left = FloorDivide(field.cameraLeft - kRecordMargin, kRecordGrid) * kRecordGrid;
    const i32 top = FloorDivide(field.cameraTop - kRecordMargin, kRecordGrid) * kRecordGrid;
    const i32 width = field.viewWidth + 2 * kRecordMargin + kRecordGrid;
    const i32 height = field.viewHeight + 2 * kRecordMargin + kRecordGrid;
    const bool moved = left != this->recordLeft || top != this->recordTop;

    // The moving roles: those drawn by a motion under way (each takes a sprite as DrawRoles meets it).
    // Every role takes a sprite (a moving one always; a still one but what it draws past its picture, its bubbles,
    // the act attached to it, its label), so that a role changing its look draws its own sprite again alone.
    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        const Role& role = state.roles[index];
        const bool spread = role.attached != nullptr || (role.flags & Role::Bubbles) != 0 ||
                            (state.leader.id == Role::NoId && role.label.has_value());
        this->recordSpriteOf[index] = this->RoleOnStep(index) || !spread ? -1 : 0;
    }

    if (this->playerMotion.active)
    {
        this->recordSpriteOf[static_cast<std::size_t>(state.playerRole)] = -1;
    }

    if (this->gpuLayers.empty())
    {
        this->gpuLayers.resize(1);
    }

    for (GpuLayer& layer : this->gpuLayers)
    {
        layer.shaded = false;
        layer.dirty = {};
    }

    GpuLayer& first = this->gpuLayers[0];
    first.role = GameState::Roles;
    first.single = true;
    first.additive = false;
    first.canvasWidth = width;
    first.canvasHeight = height;
    this->recordLeft = left;
    this->recordTop = top;
    this->recordFailed = false;

    // The roles drawn with the act a motion keeps (a stopping or turning role), as PaintBetween draws them.
    std::vector<std::pair<std::size_t, ActState>> roleActs;

    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        if (this->RoleStopping(index) || this->RoleTurning(index))
        {
            roleActs.emplace_back(index, state.roles[index].act);
            state.roles[index].act = this->roleMotions[index].act;
        }
    }

    const i32 layerOffset = view.layerOffset;
    view.layerOffset = this->placesLayer;
    const std::array<i32, 4> camera = {field.cameraLeft, field.cameraTop, field.cameraRight, field.cameraBottom};
    const std::array<int, 4> screen = {field.viewWidth, field.viewHeight, field.screenX, field.screenY};
    const int paintScale = this->paintScale;
    owner.SetDrawSize(width, height);
    const PaintKept kept(*this);
    this->replaying = true;
    field.viewWidth = width;
    field.viewHeight = height;
    field.screenX += camera[0] - left;
    field.screenY += camera[1] - top;
    field.cameraLeft = left;
    field.cameraTop = top;
    field.cameraRight = left + width;
    field.cameraBottom = top + height;
    // What lies on the original's screen is drawn where the screen lies on the recorded canvas when laid (at the
    // camera's pixel nearest where it glides; LayBetween records again as that pixel changes).
    const Point shiftKept = this->originalShift;
    this->recordOverlays = false;

    for (const MapOverlay* overlay = field.overlays.get(); overlay != nullptr; overlay = overlay->next.get())
    {
        this->recordOverlays = this->recordOverlays || overlay->parallaxX != MapOverlay::FixedToMap ||
                               overlay->parallaxY != MapOverlay::FixedToMap;
    }

    this->originalShift.x += this->gpuScreenCamera[0] - left;
    this->originalShift.y += this->gpuScreenCamera[1] - top;
    this->paintScale = kRecordParts;
    this->paintCameraX = kRecordParts * left;
    this->paintCameraY = kRecordParts * top;

    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        this->paintRoleX[index] = kRecordParts * state.roles[index].x;
        this->paintRoleY[index] = kRecordParts * state.roles[index].y;
    }

    std::size_t layers = 1;

    // A block of the colour pass painted whole; the layers it reaches are counted on the first.
    auto paintBlock = [&](std::size_t block)
    {
        this->recordPass = RecordPass::Color;
        this->recordBlock = block;
        this->recordLayer = 0;
        this->recordStillPending = false;
        this->paintColumn = static_cast<int>(block % kRecordParts);
        this->paintRow = static_cast<int>(block / kRecordParts);
        this->RecordTarget();
        this->RecordDirty(0, 0, width, height);
        this->PaintWorld();

        // What a still layer past the first holds lies anywhere on it (what follows the roles, item hints and
        // animations; a role's label, bubbles and attached act reach past its picture): what its first block holds,
        // a pixel or two wider for the others.
        if (block == 0)
        {
            layers = this->recordLayer + 1;

            for (std::size_t index = 1; index < layers; ++index)
            {
                if (this->gpuLayers[index].role >= GameState::Roles)
                {
                    this->ScanDirty(this->gpuLayers[index]);
                }
            }
        }

        kept.PutBack();
        owner.SetDrawSize(width, height);
    };

    // Diagnostics (smooth gpu): the time of each step of the recording.
    u64 stepStart = 0;
    std::array<u64, 5> theseSteps = {};

    if constexpr (BuildOptions::Diagnostics)
    {
        stepStart = SDL_GetTicksNS();
    }

    const u64 recordingStart = stepStart;
    const u64 handedBefore = recordPixelsHanded;
    const u64 changedBefore = recordLayersChanged;
    const u64 uploadBefore = recordUploadNs;

    auto step = [&](std::size_t index)
    {
        if constexpr (BuildOptions::Diagnostics)
        {
            const u64 now = SDL_GetTicksNS();
            recordSteps[index] += now - stepStart;
            theseSteps[index] += now - stepStart;
            stepStart = now;
        }
    };

    paintBlock(0);
    step(0);

    // The layers whose first block differs from what they last handed to the GPU.
    bool changed = false;

    for (std::size_t index = 0; index < layers && !this->recordFailed; ++index)
    {
        GpuLayer& layer = this->gpuLayers[index];
        const std::array<i32, 4>& dirty = layer.dirty;
        // (A sprite lies where its role is, wherever the canvas does.)
        layer.changed = (moved && layer.role >= GameState::Roles) || layer.keptRole != layer.role ||
                        layer.keptWidth != layer.canvasWidth || layer.keptHeight != layer.canvasHeight ||
                        layer.keptDirty != dirty;
        std::size_t at = 0;

        for (i32 y = dirty[1]; y < dirty[3] && !layer.changed; ++y)
        {
            const Pixel* row = layer.colors[0].Row(y) + dirty[0];
            const auto count = static_cast<std::size_t>(dirty[2] - dirty[0]);
            layer.changed = !std::equal(row, row + count, layer.kept.begin() + static_cast<std::ptrdiff_t>(at));

            // Diagnostics (smooth gpu): what changed in a sprite when the canvas moved.
            if constexpr (BuildOptions::Diagnostics)
            {
                if (layer.changed && moved && layer.role < GameState::Roles && layer.keptRole == layer.role)
                {
                    static int movedLogs = 0;
                    std::size_t differing = 0;
                    i32 firstX = -1;

                    for (std::size_t column = 0; column < count; ++column)
                    {
                        if (row[column] != layer.kept[at + column])
                        {
                            firstX = firstX < 0 ? static_cast<i32>(column) + dirty[0] : firstX;
                            ++differing;
                        }
                    }

                    if (movedLogs < 30)
                    {
                        ++movedLogs;
                        LogDiagnostic(
                            u8"moved sprite changed: role %u row %d first x %d, %u pixels of the row differ (canvas "
                            u8"%dx%d)",
                            static_cast<u32>(layer.role),
                            y,
                            firstX,
                            static_cast<u32>(differing),
                            layer.canvasWidth,
                            layer.spriteHeight
                        );
                    }
                }
            }

            at += count;
        }

        // Diagnostics (smooth gpu): why a sprite changed when the canvas moved.
        if constexpr (BuildOptions::Diagnostics)
        {
            if (moved && layer.role < GameState::Roles && layer.changed && layer.keptRole == layer.role)
            {
                static int whyLogs = 0;

                if (whyLogs < 30)
                {
                    ++whyLogs;
                    LogDiagnostic(
                        u8"moved sprite: role %u kept role %u size %dx%d kept %dx%d dirty %d,%d-%d,%d kept %d,%d-%d,%d",
                        static_cast<u32>(layer.role),
                        static_cast<u32>(layer.keptRole),
                        layer.canvasWidth,
                        layer.canvasHeight,
                        layer.keptWidth,
                        layer.keptHeight,
                        dirty[0],
                        dirty[1],
                        dirty[2],
                        dirty[3],
                        layer.keptDirty[0],
                        layer.keptDirty[1],
                        layer.keptDirty[2],
                        layer.keptDirty[3]
                    );
                }
            }
        }

        changed = changed || layer.changed;
    }

    bool whitePass = false;
    bool shadePass = false;

    if (changed && !this->recordFailed)
    {
        // What the first block held is kept to compare the next recording with.
        for (std::size_t index = 0; index < layers; ++index)
        {
            GpuLayer& layer = this->gpuLayers[index];
            const std::array<i32, 4>& dirty = layer.dirty;
            layer.kept.clear();

            for (i32 y = dirty[1]; y < dirty[3]; ++y)
            {
                const Pixel* row = layer.colors[0].Row(y) + dirty[0];
                layer.kept.insert(layer.kept.end(), row, row + (dirty[2] - dirty[0]));
            }

            layer.keptDirty = dirty;
            layer.keptRole = layer.role;
            layer.keptWidth = layer.canvasWidth;
            layer.keptHeight = layer.canvasHeight;
        }

        for (std::size_t block = 1; block < kBlocks && !this->recordFailed; ++block)
        {
            paintBlock(block);
        }

        step(1);

        // The layers past the first over white.
        for (std::size_t block = 0; block < kBlocks && (layers > 1 || this->recordOverlays) && !this->recordFailed;
             ++block)
        {
            whitePass = true;
            this->recordPass = RecordPass::White;
            this->recordBlock = block;
            this->recordLayer = 0;
            this->recordStillPending = false;
            this->recordStillPending = false;
            this->paintColumn = static_cast<int>(block % kRecordParts);
            this->paintRow = static_cast<int>(block / kRecordParts);
            this->RecordTarget();
            this->PaintWorld();
            kept.PutBack();
            owner.SetDrawSize(width, height);
        }

        step(2);
        bool shaded = false;

        for (std::size_t index = 1; index < layers; ++index)
        {
            shaded = shaded || (this->gpuLayers[index].shaded && this->gpuLayers[index].changed);
        }

        for (std::size_t block = 0; block < kBlocks && shaded && !this->recordFailed; ++block)
        {
            shadePass = true;
            this->recordPass = RecordPass::Shade;
            this->recordBlock = block;
            this->recordLayer = 0;
            this->recordStillPending = false;
            this->recordStillPending = false;
            this->paintColumn = static_cast<int>(block % kRecordParts);
            this->paintRow = static_cast<int>(block / kRecordParts);
            this->DrawRoles();
            kept.PutBack();
            owner.SetDrawSize(width, height);
        }
    }

    step(3);
    this->recordPass = RecordPass::None;
    this->recordSkip = false;
    this->replaying = false;
    this->paintScale = paintScale;
    this->originalShift = shiftKept;
    field.cameraLeft = camera[0];
    field.cameraTop = camera[1];
    field.cameraRight = camera[2];
    field.cameraBottom = camera[3];
    field.viewWidth = screen[0];
    field.viewHeight = screen[1];
    field.screenX = screen[2];
    field.screenY = screen[3];
    view.layerOffset = layerOffset;

    for (const auto& [index, act] : roleActs)
    {
        state.roles[index].act = act;
    }

    owner.SetDrawSize(Display::Width, Display::Height);

    // Diagnostics (party layers): the layers of the party and the still layer after each, as recorded.
    std::string partyLayers;
    bool afterParty = false;

    for (std::size_t index = 0; index < this->gpuLayers.size(); ++index)
    {
        GpuLayer& layer = this->gpuLayers[index];
        const std::array<i32, 4>& dirty = layer.dirty;
        const bool holds = index < layers && dirty[0] < dirty[2] && dirty[1] < dirty[3];
        layer.drawn = holds && !this->recordFailed;

        // Diagnostics (party layers): whether the layer is logged (a party role's, or the still one after it).
        bool logged = false;
        u32 partyHash = 0;

        if constexpr (BuildOptions::Diagnostics)
        {
            const bool partyLayer = layer.role < GameState::Roles && owner.state.roles[layer.role].id >= 1 &&
                                    owner.state.roles[layer.role].id <= 4;
            logged = partyLayer || afterParty;
            afterParty = partyLayer;
        }

        if (!holds)
        {
            continue;
        }

        if (layer.drawn && layer.changed)
        {
            // Diagnostics (smooth gpu): the layers handed over again.
            if constexpr (BuildOptions::Diagnostics)
            {
                ++recordLayersChanged;
                static int changeLogs = 0;

                if (changeLogs < 120)
                {
                    ++changeLogs;
                    LogDiagnostic(
                        u8"gpu layer changed: frame %u layer %u of %u role %u dirty %d,%d-%d,%d moved %d kept role %u "
                        u8"kept dirty %d,%d-%d,%d",
                        this->placesFrame,
                        static_cast<u32>(index),
                        static_cast<u32>(layers),
                        static_cast<u32>(layer.role),
                        dirty[0],
                        dirty[1],
                        dirty[2],
                        dirty[3],
                        moved ? 1 : 0,
                        static_cast<u32>(layer.keptRole),
                        layer.keptDirty[0],
                        layer.keptDirty[1],
                        layer.keptDirty[2],
                        layer.keptDirty[3]
                    );
                }
            }

            // The first layer holds the ground (what is not zoomed): its first block at the canvas's resolution.
            const i32 parts = layer.single ? 1 : kRecordParts;

            // Diagnostics (smooth gpu): the pixels handed over.
            if constexpr (BuildOptions::Diagnostics)
            {
                recordPixelsHanded +=
                    static_cast<u64>(parts * (dirty[2] - dirty[0])) * static_cast<u64>(parts * (dirty[3] - dirty[1]));
            }

            layer.left = parts * dirty[0];
            layer.top = parts * dirty[1];
            layer.width = parts * (dirty[2] - dirty[0]);
            layer.height = parts * (dirty[3] - dirty[1]);
            Display& display = owner.display;

            if (layer.single)
            {
                CopyBlock(layer.colors[0], dirty, this->gpuWoven);
            }
            else
            {
                FieldView::WeaveBlocks(layer.colors, dirty, this->gpuWoven);
            }

            // Diagnostics (party layers): the colours recorded (FNV-1a).
            if constexpr (BuildOptions::Diagnostics)
            {
                if (logged)
                {
                    partyHash = 2166136261u;

                    for (const Pixel pixel : this->gpuWoven)
                    {
                        partyHash = (partyHash ^ pixel) * 16777619u;
                    }
                }
            }

            // The first layer covers the canvas (but where the overlays under it show): it is handed over as it is; an
            // added sprite too (black adds nothing).
            if ((index == 0 && !this->recordOverlays) || layer.additive)
            {
                // Diagnostics (smooth gpu): the time of the upload.
                const u64 uploadStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
                display.UpdateLayer(
                    layer.color,
                    this->gpuWoven.data(),
                    layer.width,
                    layer.height,
                    layer.width,
                    kBlackPixel,
                    Display::LayerBlend::Opaque
                );

                if constexpr (BuildOptions::Diagnostics)
                {
                    recordUploadNs += SDL_GetTicksNS() - uploadStart;
                }
            }
            else
            {
                if (layer.single)
                {
                    CopyBlock(layer.whites[0], dirty, this->gpuWovenWhite);
                }
                else
                {
                    FieldView::WeaveBlocks(layer.whites, dirty, this->gpuWovenWhite);
                }

                // Diagnostics (smooth gpu): one layer's two recordings written out.
                if constexpr (BuildOptions::Diagnostics)
                {
                    if (const char* dump = std::getenv("SMOKE_DUMP_ROLE");
                        dump != nullptr && static_cast<std::size_t>(std::atoi(dump)) == layer.role)
                    {
                        for (int ground = 0; ground < 2; ++ground)
                        {
                            const std::vector<Pixel>& woven = ground == 0 ? this->gpuWoven : this->gpuWovenWhite;
                            std::string name =
                                "role_" + std::to_string(layer.role) + (ground == 0 ? "_black.ppm" : "_white.ppm");
                            std::FILE* file = std::fopen(name.c_str(), "wb");

                            if (file != nullptr)
                            {
                                std::fprintf(file, "P6\n%d %d\n255\n", layer.width, layer.height);

                                for (const Pixel pixel : woven)
                                {
                                    const std::array<u8, 3> rgb = {
                                        static_cast<u8>((pixel >> 11) << 3),
                                        static_cast<u8>(((pixel >> 5) & 0x3F) << 2),
                                        static_cast<u8>((pixel & 0x1F) << 3),
                                    };

                                    std::fwrite(rgb.data(), 1, rgb.size(), file);
                                }

                                std::fclose(file);
                            }
                        }

                        LogDiagnostic(
                            u8"dumped role %u: act %d frame %u picture %d flags %x",
                            static_cast<u32>(layer.role),
                            owner.state.roles[layer.role].act.id,
                            static_cast<u32>(owner.state.roles[layer.role].act.frame),
                            owner.state.roles[layer.role].act.picture,
                            owner.state.roles[layer.role].flags
                        );
                    }
                }

                // Diagnostics (smooth gpu): the time of the upload.
                const u64 uploadStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
                display.UpdateMattedLayer(
                    layer.color,
                    this->gpuWoven.data(),
                    this->gpuWovenWhite.data(),
                    layer.width,
                    layer.height,
                    layer.width
                );

                if constexpr (BuildOptions::Diagnostics)
                {
                    recordUploadNs += SDL_GetTicksNS() - uploadStart;
                }
            }

            if (layer.shaded)
            {
                if (layer.single)
                {
                    CopyBlock(layer.shades[0], dirty, this->gpuWoven);
                }
                else
                {
                    FieldView::WeaveBlocks(layer.shades, dirty, this->gpuWoven);
                }

                display.UpdateLayer(
                    layer.shade,
                    this->gpuWoven.data(),
                    layer.width,
                    layer.height,
                    layer.width,
                    kWhitePixel,
                    Display::LayerBlend::Subtract
                );
            }
        }

        // Diagnostics (party layers)
        if constexpr (BuildOptions::Diagnostics)
        {
            if (logged)
            {
                const bool roleLayer = layer.role < GameState::Roles;
                const Role* held = roleLayer ? &owner.state.roles[layer.role] : nullptr;
                partyLayers +=
                    std::to_string(index) + ":" +
                    (roleLayer
                         ? "id" + std::to_string(held->id) + "(pic " + std::to_string(held->act.frame) + "." +
                               std::to_string(held->act.picture) + " dir " + std::to_string(held->act.direction) + ")"
                         : std::string("still")) +
                    " kept " + std::to_string(layer.keptRole) + " changed " + std::to_string(layer.changed ? 1 : 0) +
                    " drawn " + std::to_string(layer.drawn ? 1 : 0) + " dirty " + std::to_string(dirty[0]) + "," +
                    std::to_string(dirty[1]) + "-" + std::to_string(dirty[2]) + "," + std::to_string(dirty[3]) +
                    (partyHash != 0 ? " hash " + std::to_string(partyHash) : std::string()) + "; ";
            }
        }

        // The first layer is painted whole each time; the others are kept clear between recordings (past the first
        // block those drawn again alone, recordSkip).
        if (index > 0)
        {
            for (std::size_t block = 0; block < kBlocks; ++block)
            {
                FillRect(layer.colors[block], dirty[0], dirty[1], dirty[2], dirty[3], kBlackPixel);

                if (!layer.changed)
                {
                    break;
                }

                if (whitePass)
                {
                    FillRect(layer.whites[block], dirty[0], dirty[1], dirty[2], dirty[3], kWhitePixel);
                }

                if (shadePass)
                {
                    FillRect(layer.shades[block], dirty[0], dirty[1], dirty[2], dirty[3], kWhitePixel);
                }
            }
        }
    }

    // Diagnostics (party layers)
    if constexpr (BuildOptions::Diagnostics)
    {
        if (!partyLayers.empty())
        {
            LogDiagnostic(u8"party layers: frame %u: %s", this->placesFrame, partyLayers.c_str());
        }
    }

    step(4);

    // Diagnostics (smooth gpu): the recordings counted.
    if constexpr (BuildOptions::Diagnostics)
    {
        ++recordStepCount;
    }

    // Diagnostics (smooth gpu): a slow recording's steps.
    if constexpr (BuildOptions::Diagnostics)
    {
        if (SDL_GetTicksNS() - recordingStart > 8'000'000)
        {
            LogDiagnostic(
                u8"slow recording (us): first block %u, other blocks %u, white %u, shade %u, hand-over %u (upload %u; "
                u8"%u "
                u8"layers changed of %u, %u kpixels; moved %d, white pass %d, shade pass %d)",
                static_cast<u32>(theseSteps[0] / 1000),
                static_cast<u32>(theseSteps[1] / 1000),
                static_cast<u32>(theseSteps[2] / 1000),
                static_cast<u32>(theseSteps[3] / 1000),
                static_cast<u32>(theseSteps[4] / 1000),
                static_cast<u32>((recordUploadNs - uploadBefore) / 1000),
                static_cast<u32>(recordLayersChanged - changedBefore),
                static_cast<u32>(layers),
                static_cast<u32>((recordPixelsHanded - handedBefore) / 1000),
                moved ? 1 : 0,
                whitePass ? 1 : 0,
                shadePass ? 1 : 0
            );
        }
    }

    // Diagnostics (smooth gpu): the steps of the recordings summed up every 40.
    if constexpr (BuildOptions::Diagnostics)
    {
        if (recordStepCount == 40)
        {
            LogDiagnostic(
                u8"gpu record steps (us): first block %u, other blocks %u, white %u, shade %u, hand-over %u (upload "
                u8"%u; "
                u8"%u layers, %u kpixels each recording)",
                static_cast<u32>(recordSteps[0] / recordStepCount / 1000),
                static_cast<u32>(recordSteps[1] / recordStepCount / 1000),
                static_cast<u32>(recordSteps[2] / recordStepCount / 1000),
                static_cast<u32>(recordSteps[3] / recordStepCount / 1000),
                static_cast<u32>(recordSteps[4] / recordStepCount / 1000),
                static_cast<u32>(recordUploadNs / recordStepCount / 1000),
                static_cast<u32>(recordLayersChanged / recordStepCount),
                static_cast<u32>(recordPixelsHanded / recordStepCount / 1000)
            );
            LogDiagnostic(
                u8"gpu record roles: %u met (%u of the game's), %u changed each recording",
                static_cast<u32>(this->diagnostics.rolesMet / recordStepCount),
                static_cast<u32>(this->diagnostics.actorsMet / recordStepCount),
                static_cast<u32>(this->diagnostics.rolesChanged / recordStepCount)
            );
            this->diagnostics.rolesMet = 0;
            this->diagnostics.actorsMet = 0;
            this->diagnostics.rolesChanged = 0;
            recordSteps = {};
            recordStepCount = 0;
            recordLayersChanged = 0;
            recordPixelsHanded = 0;
            recordUploadNs = 0;
        }
    }

    // The layers of roles no longer met are kept for a while (they come back as the camera moves), not without end.
    constexpr std::size_t kSpareLayers = 64;

    if (this->gpuLayers.size() > layers + kSpareLayers)
    {
        this->gpuLayers.erase(
            this->gpuLayers.begin() + static_cast<std::ptrdiff_t>(layers + kSpareLayers),
            this->gpuLayers.end()
        );
    }

    // A failed recording leaves nothing to compare the next with.
    if (this->recordFailed)
    {
        for (GpuLayer& layer : this->gpuLayers)
        {
            layer.keptRole = GameState::Roles + 1;
        }
    }

    return !this->recordFailed;
}

// The interface of the last game frame drawn again (as PaintBetween lays it on each block) on the canvas, twice: over
// black and over white, so that what it blends (the gold is translucent) keeps its level; handed to the GPU when it
// changed.
bool FieldView::LayInterface(int mouseX, int mouseY)
{
    Game& owner = this->game;
    Field& field = owner.field;
    Painter& painter = owner.painter;
    Scene& view = owner.scene;
    const int width = field.viewWidth;
    const int height = field.viewHeight;
    constexpr std::array<Pixel, 2> kGrounds = {0x0000, 0xFFFF};
    bool changed = false;

    for (std::size_t ground = 0; ground < kGrounds.size(); ++ground)
    {
        Surface& canvas = this->gpuInterfaceCanvas[ground];

        if (canvas.Width() != width || canvas.Height() != height)
        {
            canvas.Resize(width, height);
        }

        FillRect(canvas, 0, 0, width, height, kGrounds[ground]);
        owner.SetDrawSize(width, height);
        const PaintKept kept(*this);
        this->replaying = true;
        const int paintScale = this->paintScale;
        this->paintScale = 1;
        this->paintColumn = 0;
        this->paintRow = 0;
        const std::span<Pixel> screen(
            canvas.Pixels(),
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
        );

        auto onScreen = [&]()
        {
            owner.drawSurface = screen.data() + this->ScreenAt();
            painter.SetTarget(owner.drawSurface);
            painter.SetClipClamped(0, 0, Display::Width, Display::Height);
        };

        // The screen effects of the interface (the role heads, the pointer's sway) where they glide now.
        for (auto& [effectKey, effect] : this->effectMotions)
        {
            const u64 kind = effectKey >> kEffectKindShift;

            if (effect.seen == this->placesFrame &&
                (kind == static_cast<u64>(EffectKind::RoleHeads) || kind == static_cast<u64>(EffectKind::Pointer)))
            {
                const std::array<double, 2> place = this->EffectShown(effect);
                effect.fineX = static_cast<i32>(std::round(place[0]));
                effect.fineY = static_cast<i32>(std::round(place[1]));
            }
        }

        onScreen();
        this->BlendLines(screen.subspan(this->ScreenAt()));
        owner.talk.DrawAgain();
        onScreen();
        this->DrawRoleHeads();
        kept.PutBack();
        onScreen();
        this->DrawGold();

        if ((owner.systemFlags & Game::NameBoxesOpen) != 0)
        {
            owner.DrawNameBoxes();
        }

        if (owner.menuButtonShown)
        {
            owner.DrawMenuButton(true);
        }

        kept.PutBack();

        auto drawPointer = [&]()
        {
            if (!this->pointerShown)
            {
                return;
            }

            const u64 key = EffectKey(EffectKind::Pointer, u64{0});
            const ActState& act = view.pointerRole.act;
            const std::array<i32, 2> sway = this->EffectAt(key, this->pointerSway[0], this->pointerSway[1], true);
            painter.SetTarget(screen.data());
            painter.SetClipClamped(0, 0, width, height);
            owner.DrawPointer(mouseX + field.screenX + sway[0] + act.x, mouseY + field.screenY + sway[1] + act.y);
            kept.PutBack();
        };

        // The rebuild's map filling the window (deviations.md): the pointer drawn last, over the cleared first
        // columns of the map's rows (DrawPointerLast).
        const bool pointerLast = this->FillsWindow();

        if (!pointerLast)
        {
            drawPointer();
        }

        onScreen();
        this->DrawFinalAgain();
        kept.PutBack();

        if (pointerLast)
        {
            drawPointer();
        }

        this->replaying = false;
        this->paintScale = paintScale;
        owner.SetDrawSize(Display::Width, Display::Height);
        std::vector<Pixel>& shown = this->gpuInterfaceShown[ground];

        if (shown.size() != screen.size() || !std::equal(screen.begin(), screen.end(), shown.begin()))
        {
            shown.assign(screen.begin(), screen.end());
            changed = true;
        }
    }

    // Diagnostics (smooth gpu): the interface over black written out at a comparison.
    if constexpr (BuildOptions::Diagnostics)
    {
        if (owner.diagnostics.compare.load() != 0 && std::getenv("SMOKE_DUMP_INTERFACE") != nullptr)
        {
            const Surface& canvas = this->gpuInterfaceCanvas[0];
            const std::string name = "interface_" + std::to_string(this->placesFrame) + ".ppm";
            std::FILE* file = std::fopen(name.c_str(), "wb");

            if (file != nullptr)
            {
                std::fprintf(file, "P6\n%d %d\n255\n", canvas.Width(), canvas.Height());

                for (i32 y = 0; y < canvas.Height(); ++y)
                {
                    for (i32 x = 0; x < canvas.Width(); ++x)
                    {
                        const Pixel pixel = canvas.Row(y)[x];
                        const std::array<u8, 3> rgb = {
                            static_cast<u8>((pixel >> 11) << 3),
                            static_cast<u8>(((pixel >> 5) & 0x3F) << 2),
                            static_cast<u8>((pixel & 0x1F) << 3),
                        };

                        std::fwrite(rgb.data(), 1, rgb.size(), file);
                    }
                }

                std::fclose(file);
            }
        }
    }

    if (changed)
    {
        owner.display.UpdateMattedLayer(
            this->gpuInterface,
            this->gpuInterfaceCanvas[0].Pixels(),
            this->gpuInterfaceCanvas[1].Pixels(),
            width,
            height,
            width
        );
    }

    return true;
}

// The draws of the moving screen effects, recorded (Painter::SetRecorder): pictures as drawn, the tints of pixels as
// areas of the canvas (`pitch` pixels a row).
class FieldView::EffectRecorder final : public DrawRecorder
{
public:
    EffectRecorder(std::vector<EffectDraw>& draws, const std::array<float, 2>& shift, i32 pitch)
        : draws(draws),
          shift(shift),
          pitch(pitch)
    {
    }

    void Record(const DrawCommand& command) override
    {
        EffectDraw& draw = this->draws.emplace_back();
        draw.command = command;
        draw.shift = this->shift;
    }

    void RecordTint(std::ptrdiff_t at, std::size_t count, int red, int green, int blue) override
    {
        const auto pitch = static_cast<std::ptrdiff_t>(this->pitch);

        while (count > 0 && at >= 0 && pitch > 0)
        {
            const auto x = static_cast<i32>(at % pitch);
            const auto y = static_cast<i32>(at / pitch);
            auto width = static_cast<std::size_t>(pitch - x);
            i32 rows = 1;

            // Whole rows at once (a tint of the screen).
            if (x == 0 && count >= static_cast<std::size_t>(pitch))
            {
                rows = static_cast<i32>(count / static_cast<std::size_t>(pitch));
                width = static_cast<std::size_t>(pitch);
            }

            const std::size_t run = std::min(count, width * static_cast<std::size_t>(rows));
            EffectDraw& draw = this->draws.emplace_back();
            draw.tint = true;
            draw.shift = this->shift;
            draw.area = {x, y, static_cast<i32>(std::min(width, run)), rows};
            draw.tintLevels = {red, green, blue};
            at += static_cast<std::ptrdiff_t>(run);
            count -= run;
        }
    }

private:
    std::vector<EffectDraw>& draws;
    const std::array<float, 2>& shift;
    i32 pitch = 0;
};

// The moving screen effects drawn for the painting under way, recorded: run as PaintWorld runs them between game
// frames, the camera at its pixel nearest gpuCamera, each element drawn at the pixel under where it glides now with
// the rest of the way kept (EffectAt).
void FieldView::RecordEffects()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    Field& field = owner.field;
    Painter& painter = owner.painter;
    Scene& view = owner.scene;
    this->gpuEffects.clear();
    const std::array<i32, 4> kept = {field.cameraLeft, field.cameraTop, field.cameraRight, field.cameraBottom};
    const i32 left = this->gpuScreenCamera[0];
    const i32 top = this->gpuScreenCamera[1];
    field.cameraLeft = left;
    field.cameraTop = top;
    field.cameraRight = left + (kept[2] - kept[0]);
    field.cameraBottom = top + (kept[3] - kept[1]);
    const int paintScale = this->paintScale;
    this->paintScale = 1;
    this->paintColumn = 0;
    this->paintRow = 0;
    this->paintCameraX = left;
    this->paintCameraY = top;
    owner.SetDrawSize(field.viewWidth, field.viewHeight);
    const PaintKept putBack(*this);
    this->replaying = true;
    this->recordingEffects = true;

    if (this->gpuSpotlightMoving)
    {
        view.spotlightX = this->gpuSpotlight[0];
        view.spotlightY = this->gpuSpotlight[1];
    }

    // What an effect draws on the canvas itself goes on a canvas of no use.
    Prepare(this->effectScratch, field.viewWidth, field.viewHeight, kBlackPixel);
    owner.drawSurface = this->effectScratch.Pixels();
    painter.SetTarget(owner.drawSurface);
    painter.SetClipClamped(0, 0, field.viewWidth, field.viewHeight);

    // The rebuild's map filling the window (deviations.md): the effects kept on the map, as PaintWorld keeps them.
    this->ClipToMap();
    const bool spotlit = !state.GameFlag(GameState::PlainScreen) && state.GameFlag(GameState::Spotlight);

    if ((view.screenFlags & Scene::SpotlightClip) != 0 || spotlit)
    {
        this->ClipToSpotlight();
    }

    const auto canvasPixels = static_cast<std::size_t>(field.viewWidth) * static_cast<std::size_t>(field.viewHeight);

    if (this->distortSource.size() != canvasPixels ||
        this->distortRows.size() != static_cast<std::size_t>(field.viewHeight))
    {
        this->distortSource.resize(canvasPixels);
        std::iota(this->distortSource.begin(), this->distortSource.end(), 0);
        this->distortRows.assign(static_cast<std::size_t>(field.viewHeight), {field.viewWidth, 0});
    }

    // As PaintWorld draws them after the roles and the first animations.
    EffectRecorder recorder(this->gpuEffects, this->effectShift, field.viewWidth);
    painter.SetRecorder(&recorder);
    const bool plain = state.GameFlag(GameState::PlainScreen);
    const bool shown = (view.screenFlags & Scene::BlackScreen) == 0;

    if (shown)
    {
        this->effectShift = {};
        this->DrawSprites();
    }

    if (shown && !plain)
    {
        this->effectShift = {};
        this->DrawLeaves();
        this->effectShift = {};
        this->DrawLightning();
        this->effectShift = {};
        this->DrawFireflies();
        this->effectShift = {};
        this->DrawWeather();
    }

    // The heat haze and the ripples bend what is drawn so far (DistortPieces), under what follows.
    this->gpuEffectsBefore = this->gpuEffects.size();

    if (shown && !plain)
    {
        // They lie on the screen and change with the game frames alone: worked out once a game frame.
        const std::array<i32, 3> distortKey = {static_cast<i32>(this->placesFrame), field.viewWidth, field.viewHeight};

        if (!this->distortValid || this->distortKey != distortKey)
        {
            this->DrawHeat();
            this->DrawRipples();
            this->DistortPieces();
            this->distortKey = distortKey;
            this->distortValid = true;
        }

        this->effectShift = {};
        this->DrawSpotlight();
    }
    else
    {
        this->gpuPieces.clear();
        this->distortValid = false;
    }

    this->effectShift = {};
    this->DrawAnimations(view.animationsB);
    painter.SetRecorder(nullptr);
    putBack.PutBack();
    this->recordingEffects = false;
    this->replaying = false;
    this->paintScale = paintScale;
    field.cameraLeft = kept[0];
    field.cameraTop = kept[1];
    field.cameraRight = kept[2];
    field.cameraBottom = kept[3];
    owner.SetDrawSize(Display::Width, Display::Height);
}

// The overlays laid by the GPU (recordOverlays) as DrawOverlays draws them, the camera at its pixel nearest gpuCamera
// and each overlay's rest of the way kept.
void FieldView::RecordOverlays()
{
    Game& owner = this->game;
    Field& field = owner.field;
    Painter& painter = owner.painter;
    this->gpuOverlays.clear();

    if (!this->recordOverlays || (owner.scene.screenFlags & Scene::BlackScreen) != 0)
    {
        return;
    }

    const std::array<i32, 4> kept = {field.cameraLeft, field.cameraTop, field.cameraRight, field.cameraBottom};
    field.cameraLeft = this->gpuScreenCamera[0];
    field.cameraTop = this->gpuScreenCamera[1];
    field.cameraRight = field.cameraLeft + (kept[2] - kept[0]);
    field.cameraBottom = field.cameraTop + (kept[3] - kept[1]);
    owner.SetDrawSize(field.viewWidth, field.viewHeight);
    const PaintKept putBack(*this);
    Prepare(this->effectScratch, field.viewWidth, field.viewHeight, kBlackPixel);
    owner.drawSurface = this->effectScratch.Pixels();
    painter.SetTarget(owner.drawSurface);
    painter.SetClipClamped(0, 0, field.viewWidth, field.viewHeight);
    EffectRecorder recorder(this->gpuOverlays, this->effectShift, field.viewWidth);
    painter.SetRecorder(&recorder);
    this->recordingEffects = true;
    this->DrawOverlays();
    this->recordingEffects = false;
    painter.SetRecorder(nullptr);
    this->effectShift = {};
    putBack.PutBack();
    field.cameraLeft = kept[0];
    field.cameraTop = kept[1];
    field.cameraRight = kept[2];
    field.cameraBottom = kept[3];
    owner.SetDrawSize(Display::Width, Display::Height);
}

// Pixels `to` take the pixels `from` (both pixels from the canvas start) as a memmove would.
void FieldView::DistortMove(std::ptrdiff_t to, std::ptrdiff_t from, std::size_t count)
{
    const auto size = static_cast<std::ptrdiff_t>(this->distortSource.size());
    const auto length = static_cast<std::ptrdiff_t>(count);

    if (to < 0 || from < 0 || to + length > size || from + length > size)
    {
        return;
    }

    std::memmove(this->distortSource.data() + to, this->distortSource.data() + from, sizeof(i32) * count);
    this->DistortTouch(to, count);
}

// The ripple's refraction of the canvas as it stands (its copy of `pixels` pixels taken first).
void FieldView::DistortRipple(const ScriptObject& ripple, std::size_t pixels)
{
    const std::size_t count = std::min(
        {pixels,
         this->distortSource.size(),
         static_cast<std::size_t>(ripple.pitch) * static_cast<std::size_t>(ripple.lines)}
    );
    this->distortCopy.assign(
        this->distortSource.begin(),
        this->distortSource.begin() + static_cast<std::ptrdiff_t>(count)
    );
    ripple.Refractions(
        [this, pixels](int at, int source)
        {
            if (at >= 0 && static_cast<std::size_t>(at) < std::min(pixels, this->distortSource.size()) && source >= 0 &&
                static_cast<std::size_t>(source) < this->distortCopy.size())
            {
                this->distortSource[static_cast<std::size_t>(at)] = this->distortCopy[static_cast<std::size_t>(source)];
                this->DistortTouch(at, 1);
            }
        }
    );
}

// The rows a run of `count` pixels from `at` crosses take its columns as touched.
void FieldView::DistortTouch(std::ptrdiff_t at, std::size_t count)
{
    const std::ptrdiff_t width = this->game.field.viewWidth;
    const std::ptrdiff_t end = at + static_cast<std::ptrdiff_t>(count);

    while (at < end && width > 0)
    {
        const std::ptrdiff_t row = at / width;
        const std::ptrdiff_t column = at % width;
        const std::ptrdiff_t columnEnd = std::min(width, column + (end - at));

        if (row >= 0 && static_cast<std::size_t>(row) < this->distortRows.size())
        {
            std::array<i32, 2>& touched = this->distortRows[static_cast<std::size_t>(row)];
            touched[0] = std::min(touched[0], static_cast<i32>(column));
            touched[1] = std::max(touched[1], static_cast<i32>(columnEnd));
        }

        at += columnEnd - column;
    }
}

// The pixels that show another than their own, as runs taken from one row each; the sources made the identity again.
void FieldView::DistortPieces()
{
    this->gpuPieces.clear();
    const i32 width = this->game.field.viewWidth;

    for (std::size_t row = 0; row < this->distortRows.size(); ++row)
    {
        std::array<i32, 2>& touched = this->distortRows[row];
        const std::size_t rowStart = row * static_cast<std::size_t>(width);
        i32 column = touched[0];

        while (column < touched[1])
        {
            const i32 source = this->distortSource[rowStart + static_cast<std::size_t>(column)];

            if (source == static_cast<i32>(rowStart) + column)
            {
                ++column;
                continue;
            }

            i32 length = 1;

            while (column + length < touched[1] && (source + length) % width != 0 &&
                   this->distortSource[rowStart + static_cast<std::size_t>(column + length)] == source + length)
            {
                ++length;
            }

            this->gpuPieces.push_back(
                Display::MapPiece{
                    .sourceX = static_cast<float>(source % width),
                    .sourceY = static_cast<float>(source / width),
                    .x = static_cast<float>(column),
                    .y = static_cast<float>(row),
                    .width = static_cast<float>(length),
                    .height = 1.0f,
                }
            );
            column += length;
        }

        for (i32 at = touched[0]; at < touched[1]; ++at)
        {
            this->distortSource[rowStart + static_cast<std::size_t>(at)] = static_cast<i32>(rowStart) + at;
        }

        touched = {width, 0};
    }
}

// The picture of an effect's draw as a texture, decoded once (drawn with `decodeMode` over black and over white);
// `mask` decodes its shape alone, white.
const Display::Layer* FieldView::EffectTexture(const DrawCommand& command, u32 decodeMode, bool mask)
{
    constexpr u32 kWhitePair = 0xFFFFFFFFu;
    DrawArgument argument = command.argument;

    if (mask)
    {
        decodeMode = Painter::Silhouette | (command.mode & Painter::RleSource);
        argument.colorPair = kWhitePair;
    }

    const auto key = std::make_tuple(
        static_cast<const void*>(command.source),
        static_cast<const void*>(command.palette),
        static_cast<const void*>(argument.alphaMap),
        argument.colorPair,
        command.width,
        command.height,
        decodeMode
    );
    const auto found = this->gpuPictures.find(key);

    if (found != this->gpuPictures.end())
    {
        return &found->second;
    }

    if (command.width <= 0 || command.height <= 0)
    {
        return nullptr;
    }

    Game& owner = this->game;
    Painter& painter = owner.painter;
    const std::array<int, 2> size = {owner.drawWidth, owner.drawHeight};
    u16* const target = painter.Target();
    const std::array<int, 4> clip = painter.ClipRect();
    const u16* const palette = painter.Palette();
    constexpr std::array<Pixel, 2> kGrounds = {kBlackPixel, kWhitePixel};

    for (std::size_t ground = 0; ground < kGrounds.size(); ++ground)
    {
        Surface& surface = this->gpuDecode[ground];

        if (surface.Width() != command.width || surface.Height() != command.height)
        {
            surface.Resize(command.width, command.height);
        }

        FillRect(surface, 0, 0, command.width, command.height, kGrounds[ground]);
        owner.SetDrawSize(command.width, command.height);
        painter.SetTarget(surface.Pixels());
        painter.SetClipClamped(0, 0, command.width, command.height);
        painter.SetSource(command.source);
        painter.SetPalette(command.palette);
        painter.Options() = DrawOptions{};
        painter.DrawPicture(0, 0, command.width, command.height, decodeMode, argument);
    }

    owner.SetDrawSize(size[0], size[1]);
    painter.SetTarget(target);
    painter.SetClip(clip[0], clip[1], clip[0] + clip[2], clip[1] + clip[3]);
    painter.SetPalette(palette);
    Display::Layer& layer = this->gpuPictures[key];
    owner.display.UpdateMattedLayer(
        layer,
        this->gpuDecode[0].Pixels(),
        this->gpuDecode[1].Pixels(),
        command.width,
        command.height,
        command.width
    );
    return &layer;
}

// The recorded effects laid by the GPU over the map's layers, each picture with the blend its mode makes.
void FieldView::LayEffects(const std::vector<EffectDraw>& draws, std::size_t first, std::size_t end)
{
    Game& owner = this->game;
    const Display& display = owner.display;
    constexpr int kAlphaLevels = 16;
    constexpr int kTop = 0xFF;

    for (std::size_t index = first; index < end && index < draws.size(); ++index)
    {
        const EffectDraw& draw = draws[index];

        if (draw.tint)
        {
            display.TintArea(
                static_cast<float>(draw.area[0]) + draw.shift[0],
                static_cast<float>(draw.area[1]) + draw.shift[1],
                static_cast<float>(draw.area[2]),
                static_cast<float>(draw.area[3]),
                draw.tintLevels[0],
                draw.tintLevels[1],
                draw.tintLevels[2]
            );
            continue;
        }

        const DrawCommand& command = draw.command;
        const bool zoomed = (command.mode & Painter::ZoomModeMask) >= Painter::ZoomModes;
        const u32 effect = zoomed ? (command.mode & Painter::EffectMask) : (command.mode & Painter::EffectMask);
        const u32 rle = command.mode & Painter::RleSource;
        u32 decodeMode = Painter::Copy | rle;
        Display::LayerBlend blend = Display::LayerBlend::Over;
        Display::LayerLook look;
        look.mirrored = (command.mode & Painter::Mirror) != 0;
        look.flipped = (command.mode & Painter::Flip) != 0;
        bool mask = false;
        bool shown = true;

        switch (effect)
        {
            case Painter::Copy:
            case Painter::ZoomCopy:
            case Painter::FadeDown:
                break;

            case Painter::EdgeMark:
            case Painter::AlphaMap:
            case Painter::Silhouette:
            case Painter::PaletteCopy:
                decodeMode = effect | rle;
                break;

            case Painter::ZoomSilhouette:
                decodeMode = Painter::Silhouette | rle;
                break;

            case Painter::Additive:
            case Painter::ScaledGlow:
            case Painter::ZoomAddTinted:
                blend = Display::LayerBlend::Add;
                break;

            case Painter::Subtractive:
            case Painter::ZoomSubtractTinted:
                blend = Display::LayerBlend::Subtract;
                break;

            case Painter::Translucent:
            case Painter::ZoomTranslucent:
                if (command.options.alpha <= 0)
                {
                    shown = false;
                }
                else if (command.options.alpha < kAlphaLevels - 1)
                {
                    look.alpha = static_cast<u8>(command.options.alpha * kTop / kAlphaLevels);
                }

                break;

            case Painter::Shadow:
            case Painter::TintUnder:
            case Painter::ZoomTint:
                mask = true;
                break;

            default:
                // What turns the destination grey or smears it is left out.
                shown = false;
                break;
        }

        if (!shown)
        {
            continue;
        }

        const Display::Layer* texture = this->EffectTexture(command, decodeMode, mask);

        if (texture == nullptr)
        {
            continue;
        }

        float x = static_cast<float>(command.x) + draw.shift[0];
        float y = static_cast<float>(command.y) + draw.shift[1];
        auto width = static_cast<float>(command.width);
        auto height = static_cast<float>(command.height);

        if (zoomed)
        {
            constexpr float kUnit = 1024.0f;
            const float zoomX = static_cast<float>(command.zoomX) / kUnit;
            const float zoomY = static_cast<float>(command.zoomY) / kUnit;
            const auto anchorX =
                static_cast<float>(look.mirrored ? command.width - command.zoomAnchorX : command.zoomAnchorX);
            const auto anchorY =
                static_cast<float>(look.flipped ? command.height - command.zoomAnchorY : command.zoomAnchorY);
            x += anchorX - anchorX * zoomX;
            y += anchorY - anchorY * zoomY;
            width *= zoomX;
            height *= zoomY;
        }
        else if (command.options.scaleHeight != 0)
        {
            height = static_cast<float>(command.options.scaleHeight);
        }

        display.ClipLayers(command.clip[0], command.clip[1], command.clip[2], command.clip[3]);

        // The shape tinting what is under it: its positive steps added, its negative ones taken away.
        const std::array<int, 3> levels =
            {command.options.tintRed, command.options.tintGreen, command.options.tintBlue};

        auto tintUnder = [&display, &levels, x, y, width, height](const Display::Layer& shape, Display::LayerLook look)
        {
            // The steps as Display::TintArea takes them (a step of the 5-bit channel each).
            auto amount = [](int level)
            {
                constexpr int kTintSteps = 31;
                constexpr int kFull = 0xFF;
                return static_cast<u8>(std::min(std::max(level, 0), kTintSteps) * kFull / kTintSteps);
            };

            look.red = amount(levels[0]);
            look.green = amount(levels[1]);
            look.blue = amount(levels[2]);
            display.DrawLayer(shape, x, y, width, height, Display::LayerBlend::Add, look);
            look.red = amount(-levels[0]);
            look.green = amount(-levels[1]);
            look.blue = amount(-levels[2]);
            display.DrawLayer(shape, x, y, width, height, Display::LayerBlend::Subtract, look);
        };

        if (mask)
        {
            tintUnder(*texture, look);
            continue;
        }

        display.DrawLayer(*texture, x, y, width, height, blend, look);

        // An added picture is tinted after it is added (Painter::Additive, ScaledGlow, BlitZoomAddTinted).
        if (blend == Display::LayerBlend::Add && levels != std::array<int, 3>{})
        {
            if (const Display::Layer* shape = this->EffectTexture(command, decodeMode, true); shape != nullptr)
            {
                tintUnder(*shape, look);
            }
        }
    }

    display.UnclipLayers();
}

bool FieldView::LayBetween(int scale)
{
    // Diagnostics (smooth gpu): the time of the laying and of its recording.
    const u64 layStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
    u64 recordedNs = 0;
    Game& owner = this->game;
    GameState& state = owner.state;
    const Field& field = owner.field;
    const Scene& view = owner.scene;
    const auto canvasPixels = static_cast<std::size_t>(field.viewWidth) * static_cast<std::size_t>(field.viewHeight);

    // A game frame not drawn on this canvas (none yet, or the window resized since) is painted by PaintBetween.
    if (this->finalLayer.size() != canvasPixels)
    {
        // Diagnostics (smooth gpu): a frame size the canvas does not have.
        if constexpr (BuildOptions::Diagnostics)
        {
            static std::size_t loggedSize = 1;

            if (loggedSize != this->finalLayer.size())
            {
                LogDiagnostic(
                    u8"gpu layers: last frame %u pixels, canvas %u (%dx%d)",
                    static_cast<u32>(this->finalLayer.size()),
                    static_cast<u32>(canvasPixels),
                    field.viewWidth,
                    field.viewHeight
                );
                loggedSize = this->finalLayer.size();
            }
        }

        this->gpuValid = false;
        return false;
    }

    // A game frame the movie played in, or one before the places are known, is shown as drawn, scaled by the GPU.
    if (this->movie.active != 0 || this->movieFrame || !this->placesValid)
    {
        if (this->gpuFinalShown != this->finalLayer)
        {
            this->gpuFinalShown = this->finalLayer;
            owner.display.UpdateLayer(
                this->gpuFinal,
                this->finalLayer.data(),
                field.viewWidth,
                field.viewHeight,
                field.viewWidth,
                0,
                Display::LayerBlend::Opaque
            );
        }

        owner.display.BeginLayers(scale);
        owner.display.DrawLayer(
            this->gpuFinal,
            0.0f,
            0.0f,
            static_cast<float>(field.viewWidth),
            static_cast<float>(field.viewHeight),
            Display::LayerBlend::Opaque
        );
        owner.display.EndLayers();
        this->gpuValid = false;
        return true;
    }

    // Where the camera and the spotlight glide now.
    auto placeCamera = [this, scale]()
    {
        const std::array<i32, 2> fineCamera = this->CameraFine(scale);
        this->gpuCamera = {
            static_cast<float>(fineCamera[0] + this->shakeShift[0] * scale) / static_cast<float>(scale),
            static_cast<float>(fineCamera[1] + this->shakeShift[1] * scale) / static_cast<float>(scale),
        };

        this->gpuScreenCamera = {
            static_cast<i32>(std::floor(this->gpuCamera[0] + 0.5f)),
            static_cast<i32>(std::floor(this->gpuCamera[1] + 0.5f)),
        };
    };

    placeCamera();

    const auto spotlightMotion = this->effectMotions.find(EffectKey(EffectKind::Spotlight, u64{0}));
    this->gpuSpotlightMoving =
        spotlightMotion != this->effectMotions.end() && spotlightMotion->second.seen == this->placesFrame;

    if (this->gpuSpotlightMoving)
    {
        const std::array<double, 2> light = this->EffectShown(spotlightMotion->second);
        this->gpuSpotlight = {static_cast<i32>(std::round(light[0])), static_cast<i32>(std::round(light[1]))};
    }

    const bool spotlit = !state.GameFlag(GameState::PlainScreen) && state.GameFlag(GameState::Spotlight);
    const bool spotlightClipped = (view.screenFlags & Scene::SpotlightClip) != 0 || spotlit;
    // What lies on the original's screen within the layers (the spotlight, the story's animations) stays on the
    // screen as the camera glides: the layers recorded again at each pixel of the camera.
    const bool screenFixed = spotlightClipped || view.animationsA != nullptr || view.animationsB != nullptr;

    // The layers are recorded again for another game frame, or when a moving role starts or stops or the act a
    // motion keeps changes.
    std::vector<i32>& key = this->gpuKeyNow;
    key.clear();
    key.push_back(static_cast<i32>(this->placesFrame));
    // The camera moves within the recorded canvas between game frames (a smooth step of the player): recorded again
    // when the canvas's corner moves to another cell of its grid (RecordLayers).
    key.push_back(FloorDivide(field.cameraLeft - kRecordMargin, kRecordGrid));
    key.push_back(FloorDivide(field.cameraTop - kRecordMargin, kRecordGrid));
    key.push_back(this->playerMotion.active ? 1 : 0);

    if (screenFixed)
    {
        key.push_back(this->gpuScreenCamera[0]);
        key.push_back(this->gpuScreenCamera[1]);
        key.push_back(this->gpuSpotlight[0]);
        key.push_back(this->gpuSpotlight[1]);
    }

    for (std::size_t index = 0; index < GameState::Roles; ++index)
    {
        if (this->RoleOnStep(index))
        {
            const bool keptAct = this->RoleStopping(index) || this->RoleTurning(index);
            const ActState& act = keptAct ? this->roleMotions[index].act : state.roles[index].act;
            key.push_back(static_cast<i32>(index));
            key.push_back(keptAct ? 1 : 0);
            key.push_back(static_cast<i32>(act.frame));
            key.push_back(act.picture);
            key.push_back(act.direction);
        }
    }

    if (!this->gpuValid || key != this->gpuKey)
    {
        // Diagnostics (smooth gpu): what changed the key.
        if constexpr (BuildOptions::Diagnostics)
        {
            static int keyLogs = 0;

            if (keyLogs < 400)
            {
                ++keyLogs;
                std::string changed;

                for (std::size_t at = 0; at < std::max(key.size(), this->gpuKey.size()); ++at)
                {
                    const i32 before = at < this->gpuKey.size() ? this->gpuKey[at] : INT_MIN;
                    const i32 now = at < key.size() ? key[at] : INT_MIN;

                    if (before != now)
                    {
                        changed += std::to_string(at) + ":" + std::to_string(before) + ">" + std::to_string(now) + " ";
                    }
                }

                int parallax = 0;

                for (const MapOverlay* overlay = field.overlays.get(); overlay != nullptr;
                     overlay = overlay->next.get())
                {
                    parallax +=
                        overlay->parallaxX != MapOverlay::FixedToMap || overlay->parallaxY != MapOverlay::FixedToMap;
                }

                LogDiagnostic(u8"gpu key: map %d parallax overlays %d", owner.currentMap, parallax);
                LogDiagnostic(
                    u8"gpu key: valid %d size %u frame %u clock %u changed %s",
                    this->gpuValid ? 1 : 0,
                    static_cast<u32>(key.size()),
                    this->placesFrame,
                    this->motionClock,
                    AsUtf8(changed.c_str())
                );
            }
        }

        this->gpuKey = key;
        // Diagnostics (smooth gpu): the time of the recording.
        const u64 recordStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
        this->gpuValid = this->RecordLayers();

        if constexpr (BuildOptions::Diagnostics)
        {
            recordedNs = SDL_GetTicksNS() - recordStart;
            gpuTimes[0] += recordedNs;
            ++gpuCounts[0];

            if (!this->gpuValid)
            {
                LogDiagnostic(u8"gpu layers: recording failed at frame %u", this->placesFrame);
            }
        }

        if (!this->gpuValid)
        {
            return false;
        }

        // A recording that keeps the frame from the display's next refresh: what moves is shown where it is once it
        // is over (one that does not leaves the frame as it began, like the others); what lies on the screen within
        // the layers was recorded for the camera's place before.
        const u64 period = owner.display.RefreshPeriod();
        const bool late = period == 0 || SDL_GetTicksNS() - owner.display.LastShown() > period;

        if (!screenFixed && late)
        {
            owner.AdvanceMotion();
            placeCamera();
        }
    }

    // The layers laid where the camera and the moving roles are now (canvas pixels, fractions kept to the pixels of
    // the composing copy, so that every layer lies on its pixels alike).
    float cameraX = this->gpuCamera[0];
    float cameraY = this->gpuCamera[1];
    const float half = 1.0f / static_cast<float>(kRecordParts);
    int mouseX = 0;
    int mouseY = 0;
    owner.input.PeekMouse(mouseX, mouseY);
    // The effects moving between game frames, recorded where they are now.
    // Diagnostics (smooth gpu): the time of the effects.
    const u64 effectsStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
    this->RecordEffects();

    if constexpr (BuildOptions::Diagnostics)
    {
        gpuTimes[4] += SDL_GetTicksNS() - effectsStart;
    }

    const Display& display = owner.display;
    display.BeginLayers(scale);
    const auto grid = static_cast<float>(display.ComposeScale());
    cameraX = std::round(cameraX * grid) / grid;
    cameraY = std::round(cameraY * grid) / grid;

    // What the heat haze and the ripples bend is drawn on a copy of the screen first.
    if (!this->gpuPieces.empty())
    {
        owner.display.BeginMapTarget();
    }

    // The overlays under the map (recordOverlays), the pictures decoded again for another map's.
    if (this->gpuOverlaysOf != field.overlays.get())
    {
        this->gpuPictures.clear();
        this->gpuOverlaysOf = field.overlays.get();
    }

    this->RecordOverlays();

    this->LayEffects(this->gpuOverlays, 0, this->gpuOverlays.size());

    // The rebuild's map filling the window (deviations.md): every layer kept on the map, as the original's screen
    // never lies past it (a moving role's sprite carries what its act draws round it, the sparkles of 仙界傳送點
    // past the edge of a map narrower than the window); x, y, right, bottom on the canvas.
    const bool mapClipped = this->FillsWindow();
    const std::array<int, 4> mapArea = {
        static_cast<int>(std::ceil(-cameraX)),
        static_cast<int>(std::ceil(-cameraY)),
        static_cast<int>(std::floor(static_cast<float>(MapHeader::TileSize * owner.state.map.width) - cameraX)),
        static_cast<int>(std::floor(static_cast<float>(MapHeader::TileSize * owner.state.map.height) - cameraY)),
    };

    for (const GpuLayer& layer : this->gpuLayers)
    {
        if (!layer.drawn)
        {
            continue;
        }

        float x = static_cast<float>(this->recordLeft) - cameraX;
        float y = static_cast<float>(this->recordTop) - cameraY;

        if (layer.role < GameState::Roles)
        {
            const std::array<i32, 2> place = this->RoleFine(layer.role, scale);
            x = static_cast<float>(place[0]) / static_cast<float>(scale) - cameraX - static_cast<float>(layer.originX);
            y = static_cast<float>(place[1]) / static_cast<float>(scale) - cameraY - static_cast<float>(layer.originY);
        }

        const float unit = layer.single ? 1.0f : half;

        // Diagnostics (party overlap): where the party's layers are laid (logged when a place changes).
        if constexpr (BuildOptions::Diagnostics)
        {
            if (layer.role < GameState::Roles && layer.role < this->roleMotions.size() &&
                owner.state.roles[layer.role].id >= 1 && owner.state.roles[layer.role].id <= 4)
            {
                static std::array<std::array<i32, 3>, GameState::Roles> lastLaid = {};
                const std::array<i32, 2> fine = this->RoleFine(layer.role, scale);
                const auto layerIndex = static_cast<i32>(&layer - this->gpuLayers.data());
                std::array<i32, 3>& last = lastLaid[layer.role];

                if (last[0] != fine[0] || last[1] != fine[1] || last[2] != layerIndex)
                {
                    last = {fine[0], fine[1], layerIndex};
                    const Role& laid = owner.state.roles[layer.role];
                    const RoleMotion& step = this->roleMotions[layer.role];
                    LogDiagnostic(
                        u8"party laid: frame %u role %u id %u layer %d fine %d,%d (1/%d) at %d,%d origin %d,%d step "
                        u8"%d/%d "
                        u8"active %d",
                        owner.gameFrames,
                        static_cast<u32>(layer.role),
                        static_cast<u32>(laid.id),
                        layerIndex,
                        fine[0],
                        fine[1],
                        scale,
                        laid.x,
                        laid.y,
                        layer.originX,
                        layer.originY,
                        step.elapsed,
                        step.length,
                        step.active ? 1 : 0
                    );
                }
            }
        }

        x += static_cast<float>(layer.left) * unit;
        y += static_cast<float>(layer.top) * unit;
        const float shownWidth = static_cast<float>(layer.width) * unit;
        const float shownHeight = static_cast<float>(layer.height) * unit;
        const bool lit = spotlightClipped && layer.role < GameState::Roles;
        const bool clipped = lit || mapClipped;

        // A moving role is clipped to the spotlight where the light is now; with the map filling the window every
        // layer to the map too.
        if (clipped)
        {
            std::array<int, 4> area =
                mapClipped ? mapArea : std::array<int, 4>{0, 0, field.viewWidth, field.viewHeight};

            if (lit)
            {
                const i32 lightX =
                    (this->gpuSpotlightMoving ? this->gpuSpotlight[0] : view.spotlightX) + this->originalShift.x;
                const i32 lightY =
                    (this->gpuSpotlightMoving ? this->gpuSpotlight[1] : view.spotlightY) + this->originalShift.y;
                area = {
                    std::max(area[0], lightX - Scene::SpotlightRadius),
                    std::max(area[1], lightY - Scene::SpotlightRadius),
                    std::min(area[2], lightX + Scene::SpotlightRadius),
                    std::min(area[3], lightY + Scene::SpotlightRadius),
                };
            }

            display.ClipLayers(area[0], area[1], std::max(area[2] - area[0], 0), std::max(area[3] - area[1], 0));
        }

        if (layer.shaded)
        {
            display.DrawLayer(layer.shade, x, y, shownWidth, shownHeight, Display::LayerBlend::Subtract);
        }

        const bool opaque = &layer == this->gpuLayers.data() && !this->recordOverlays;

        // Diagnostics (smooth gpu): each layer laid, at a comparison asked by the Linux test program.
        if constexpr (BuildOptions::Diagnostics)
        {
            if (owner.diagnostics.compare.load() != 0)
            {
                LogDiagnostic(
                    u8"compare layer %u role %u id %u single %d additive %d at %d,%d size %dx%d (tenths) shaded %d",
                    static_cast<u32>(&layer - this->gpuLayers.data()),
                    static_cast<u32>(layer.role),
                    layer.role < GameState::Roles ? static_cast<u32>(owner.state.roles[layer.role].id) : 0u,
                    layer.single ? 1 : 0,
                    layer.additive ? 1 : 0,
                    static_cast<i32>(x * 10.0f),
                    static_cast<i32>(y * 10.0f),
                    static_cast<i32>(shownWidth * 10.0f),
                    static_cast<i32>(shownHeight * 10.0f),
                    layer.shaded ? 1 : 0
                );
            }
        }

        display.DrawLayer(
            layer.color,
            x,
            y,
            shownWidth,
            shownHeight,
            opaque           ? Display::LayerBlend::Opaque
            : layer.additive ? Display::LayerBlend::Add
                             : Display::LayerBlend::Over
        );

        if (clipped)
        {
            display.UnclipLayers();
        }
    }

    // The effects over the map, bent with it by the heat haze and the ripples up to those that follow them.
    this->LayEffects(this->gpuEffects, 0, this->gpuEffectsBefore);

    if (!this->gpuPieces.empty())
    {
        owner.display.EndMapTarget(this->gpuPieces);
    }

    this->LayEffects(this->gpuEffects, this->gpuEffectsBefore, this->gpuEffects.size());

    // Diagnostics (smooth gpu): the time of the effects laid.
    if constexpr (BuildOptions::Diagnostics)
    {
        gpuTimes[3] += SDL_GetTicksNS() - effectsStart;
        ++gpuCounts[3];
    }

    // The rebuild's map filling the window (deviations.md): the map's edge inside the canvas cleared (lowered to black)
    // before the fade, where the map lies now (ClearMapEdge).
    if (const int edge = this->MapEdgeAt(cameraX); edge >= 0)
    {
        constexpr int kToBlack = -31;  // every channel lowered all the way (TintArea saturates)
        display.TintArea(
            -cameraX,
            0.0f,
            static_cast<float>(FieldView::StrayColumns),
            static_cast<float>(field.viewHeight),
            kToBlack,
            kToBlack,
            kToBlack
        );
    }

    // The fade of the last game frame over its map (PaintBetween tints the map alone, under the interface).
    if (this->fadeTinted)
    {
        display.TintArea(
            0.0f,
            0.0f,
            static_cast<float>(field.viewWidth),
            static_cast<float>(field.viewHeight),
            this->fadeLevels.red,
            this->fadeLevels.green,
            this->fadeLevels.blue
        );
    }

    // Diagnostics (smooth gpu): the time of the interface.
    const u64 interfaceStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
    this->LayInterface(mouseX, mouseY);

    if constexpr (BuildOptions::Diagnostics)
    {
        gpuTimes[1] += SDL_GetTicksNS() - interfaceStart;
        ++gpuCounts[1];
    }

    display.DrawLayer(
        this->gpuInterface,
        0.0f,
        0.0f,
        static_cast<float>(field.viewWidth),
        static_cast<float>(field.viewHeight),
        Display::LayerBlend::Over
    );
    // Diagnostics (smooth gpu): the time of presenting.
    const u64 presentStart = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;
    owner.display.EndLayers();
    const u64 shownNs = BuildOptions::Diagnostics ? SDL_GetTicksNS() : 0;

    if constexpr (BuildOptions::Diagnostics)
    {
        gpuTimes[2] += shownNs - presentStart;
    }

    // Diagnostics (smooth gpu): the time since the frame shown before, and what a slow one did.
    if constexpr (BuildOptions::Diagnostics)
    {
        if (lastShownNs != 0)
        {
            const u64 interval = shownNs - lastShownNs;
            constexpr std::array<u64, 4> kBounds = {5'000'000, 9'000'000, 13'000'000, 17'000'000};
            std::size_t bucket = 0;

            while (bucket < kBounds.size() && interval >= kBounds[bucket])
            {
                ++bucket;
            }

            ++intervalCounts[bucket];
            intervalMostNs = std::max(intervalMostNs, interval);

            if (interval >= 13'000'000)
            {
                LogDiagnostic(
                    u8"slow frame: %u us since the last shown, game frame %s, this one %u us (recording %u us, "
                    u8"present %u us)",
                    static_cast<u32>(interval / 1000),
                    owner.gameFrames != lastShownGameFrame ? "ran" : "none",
                    static_cast<u32>((shownNs - layStart) / 1000),
                    static_cast<u32>(recordedNs / 1000),
                    static_cast<u32>((shownNs - presentStart) / 1000)
                );
            }
        }
    }

    // Diagnostics (smooth gpu): the frame shown, for the time to the next.
    if constexpr (BuildOptions::Diagnostics)
    {
        lastShownNs = shownNs;
        lastShownGameFrame = owner.gameFrames;
    }

    // Diagnostics (smooth gpu): when each frame was shown and the camera it showed, while it moves.
    if constexpr (BuildOptions::Diagnostics)
    {
        static int cameraLines = 0;
        static std::array<float, 2> lastCamera = {};

        if (cameraLines < 4000 && owner.currentMap == 34 && this->gpuCamera != lastCamera)
        {
            ++cameraLines;
            LogDiagnostic(
                u8"cam %u %d %d",
                static_cast<u32>(shownNs / 1000),
                static_cast<i32>(this->gpuCamera[0] * static_cast<float>(scale)),
                static_cast<i32>(this->gpuCamera[1] * static_cast<float>(scale))
            );
        }

        lastCamera = this->gpuCamera;
    }

    // Diagnostics (smooth gpu): the times summed up every 300 frames laid.
    if constexpr (BuildOptions::Diagnostics)
    {
        if (gpuCounts[1] == 300)
        {
            LogDiagnostic(
                u8"gpu layers: %u recordings %u us, interface %u us, effects %u us (record %u us, %u draws, %u "
                u8"pieces), "
                u8"present %u us, layers %u",
                static_cast<u32>(gpuCounts[0]),
                static_cast<u32>(gpuCounts[0] != 0 ? gpuTimes[0] / gpuCounts[0] / 1000 : 0),
                static_cast<u32>(gpuTimes[1] / gpuCounts[1] / 1000),
                static_cast<u32>(gpuTimes[3] / gpuCounts[1] / 1000),
                static_cast<u32>(gpuTimes[4] / gpuCounts[1] / 1000),
                static_cast<u32>(this->gpuEffects.size()),
                static_cast<u32>(this->gpuPieces.size()),
                static_cast<u32>(gpuTimes[2] / gpuCounts[1] / 1000),
                static_cast<u32>(std::count_if(
                    this->gpuLayers.begin(),
                    this->gpuLayers.end(),
                    [](const GpuLayer& layer)
                    {
                        return layer.drawn;
                    }
                ))
            );
            LogDiagnostic(
                u8"gpu frame intervals: <5ms %u, <9ms %u, <13ms %u, <17ms %u, longer %u, most %u us",
                static_cast<u32>(intervalCounts[0]),
                static_cast<u32>(intervalCounts[1]),
                static_cast<u32>(intervalCounts[2]),
                static_cast<u32>(intervalCounts[3]),
                static_cast<u32>(intervalCounts[4]),
                static_cast<u32>(intervalMostNs / 1000)
            );
            intervalCounts = {};
            intervalMostNs = 0;
            gpuTimes = {};
            gpuCounts = {};
        }
    }

    return true;
}

}  // namespace swd3e
