#include "map/field_view_internal.h"

namespace swd3e
{

// swd3e.exe 0x00413E80: the roles of the three layers (2, 0, then 1), 70 rows from 20 above the camera, with their
// sounds
void FieldView::DrawRoles()
{
    constexpr std::array<int, 3> kLayerOrder = {2, 0, 1};
    const GameState& state = this->game.state;

    for (const int layer : kLayerOrder)
    {
        const std::vector<Role*>& rows = this->game.field.rows[static_cast<std::size_t>(layer)];
        // The rows compare signed (jge): those above the map (roles with y < 0) are drawn too.
        int row = this->game.field.cameraTop / MapHeader::TileSize - Field::RowMargin;

        for (int count = 0; count < this->ViewRows() + 2 * Field::RowMargin; ++count, ++row)
        {
            const int index = row + Field::RowMargin;

            if (row >= state.map.height + Field::RowMargin || index < 0 ||
                static_cast<std::size_t>(index) >= rows.size())
            {
                continue;
            }

            for (Role* role = rows[static_cast<std::size_t>(index)]; role != nullptr; role = role->next)
            {
                this->DrawRole(*role);

                if (role->sound != 0 && !this->replaying)
                {
                    this->UpdateRoleSound(*role);
                }
            }
        }
    }
}

// swd3e.exe 0x00413F20: a visible role (flag 0x8000 without 0x400) near the camera: its sound cue, its shadow, its
// picture with its wave and translucency, a tinted copy (the map's tint and the tint of bits 20..23), the act attached
// to it, its bubbles and, while no event runs, its label. Act roles are skipped while story flag 0xB is set. A role
// with an id and without Role::NoDepthScale is zoomed by the map's depth scale where it stands (between its tile row
// and the one above for a scale by rows), its shadow too; the zoom is reset afterwards.
int FieldView::DrawRole(Role& role)
{
    if ((role.flags & (Role::Visible | Role::Hidden)) != Role::Visible)
    {
        return 0;
    }

    Painter& painter = this->game.painter;
    const Field& field = this->game.field;
    const Scene& view = this->game.scene;
    const int x = role.x;
    const int y = role.y;
    const int cameraLeft = field.cameraLeft;
    const int cameraTop = field.cameraTop;

    if (x - cameraLeft < -kDrawnRoleMargin || x - cameraLeft > field.viewWidth + kDrawnRoleMargin)
    {
        return 1;
    }

    u32 frame = kNoFrame;

    if (role.act.id != 0)
    {
        frame = role.act.frame;

        if (this->game.state.GameFlag(GameState::HideActRoles))
        {
            return 1;
        }
    }

    if (role.act.view != 0 && !this->replaying)
    {
        this->game.PlaySoundAt(role.act.view, x, y);
        role.act.view = 0;
    }

    const Picture* picture = this->game.archive.GetPicture(frame, role.act.picture);

    if (picture == nullptr)
    {
        return 1;
    }

    int scale = MapScale::DefaultScale;
    u32 mode = role.act.flags;
    bool zoomed = false;

    if (role.id != Role::NoId && (role.flags & Role::NoDepthScale) == 0)
    {
        const MapScale& depth = field.scale;
        scale = depth.Scale(depth.LevelAt(role.x, role.y));

        if (depth.GetKind() == MapScale::Kind::Rows)
        {
            const int above = depth.Scale(depth.LevelAt(role.x, role.y - MapHeader::TileSize));
            scale = above + (role.y & MapHeader::TileFraction) * (scale - above) / MapHeader::TileSize;
        }

        zoomed = true;
        mode |= Painter::ZoomCopy;
    }

    if (field.shadowKind != 0 && (role.flags & Role::CellEffects) != 0)
    {
        this->DrawShadow(
            role,
            *picture,
            x - role.act.x - cameraLeft,
            y - role.act.y - cameraTop,
            field.shadowKind,
            kShadowTint[(role.flags & Role::CellShades) >> Role::CellShadeShift],
            scale
        );
    }

    painter.SetWave(role.act.wait);
    painter.SetWavePhase(role.act.wavePhase);
    DrawOptions& options = painter.Options();
    options.unusedFlag = 0;
    options.alpha = role.act.signal;

    if (scale != 0)
    {
        painter.SetZoomSheared(role.act.x + kZoomAnchorX, role.act.y + kZoomAnchorY, scale, scale, 0);
    }

    options.tintBlue = 0;
    options.tintGreen = 0;
    options.tintRed = 0;
    painter.SetSource(picture->data.data());
    const int drawX = x + role.drawOffsetX - role.act.x - cameraLeft;
    const int drawY = y + role.drawOffsetY - role.act.y - cameraTop;
    painter.DrawPicture(drawX, drawY, picture->width, picture->height, mode, PictureArgument(*picture));
    int red = 0;
    int green = 0;
    int blue = 0;

    if ((role.flags & Role::CellEffects) != 0)
    {
        red = view.cellTintRed;
        green = view.cellTintGreen;
        blue = view.cellTintBlue;
    }

    if ((role.flags & Role::CellShades) != 0)
    {
        const int tint = kRoleTint[(role.flags & Role::CellShades) >> Role::CellShadeShift];
        red += tint;
        green += tint;
        blue += tint;
    }

    options.tintRed = red;
    options.tintGreen = green;
    options.tintBlue = blue;

    if (red != 0 || green != 0 || blue != 0)
    {
        painter.SetSource(picture->data.data());
        painter.DrawPicture(
            drawX,
            drawY,
            picture->width,
            picture->height,
            (mode & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::TintUnder |
                (zoomed ? Painter::ZoomCopy : 0u),
            PictureArgument(*picture)
        );
    }

    if (role.attached != nullptr)
    {
        const ActState& attached = *role.attached;
        const Picture* extra = this->game.archive.GetPicture(attached.frame, attached.picture);

        if (extra != nullptr)
        {
            painter.SetSource(extra->data.data());
            painter.DrawPicture(
                role.drawOffsetX - attached.x - cameraLeft + x + kAttachedShift,
                y + role.drawOffsetY - scale * role.act.y / kZoomUnit - cameraTop,
                extra->width,
                extra->height,
                attached.flags
            );
        }
    }

    painter.SetZoom(0, 0, kZoomUnit, kZoomUnit);

    if (!this->replaying)
    {
        role.act.wavePhase = static_cast<u8>(painter.WavePhase());
    }

    if ((role.flags & Role::Bubbles) != 0 && !this->game.state.GameFlag(GameState::PlainScreen))
    {
        this->DrawBubbles(x, y, role.id);
    }

    if (this->game.state.leader.id == Role::NoId && role.label.has_value())
    {
        const auto length = static_cast<int>(role.label->size());
        const auto color = static_cast<Pixel>(this->game.textColors.primary[static_cast<std::size_t>(role.labelColor)]);
        this->game.largeFont.DrawText(
            this->game.drawSurface,
            role.x - kLabelGlyphWidth * length / 2 - cameraLeft + kLabelShift,
            role.y - role.act.y - cameraTop,
            *role.label,
            color,
            kLabelStyle
        );
    }

    return 1;
}

// swd3e.exe 0x004143B0: the sound of a role near the controlled one (within 512 pixels while visible): it is played
// once its timer runs out (a period of 0xFFFF plays it once unlooped, marked by flag 0x1000000) at a volume (not scaled
// by the sound volume setting in swd3e) and pan by the distance;
// out of range a held sound is stopped
void FieldView::UpdateRoleSound(Role& role)
{
    GameState& state = this->game.state;
    Scene& view = this->game.scene;
    SoundEffects& effects = this->game.audio.Effects();
    const Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];
    // 0x004143D7..0x004143E8: the differences and the sum of their squares wrap in 32 bits (sub, imul, add).
    const auto dx = static_cast<u32>(role.x) - static_cast<u32>(player.x);
    const auto dy = static_cast<u32>(role.y) - static_cast<u32>(player.y);
    const auto squared = static_cast<i32>(dx * dx + dy * dy);
    const auto distance = static_cast<int>(crt::Ftol(std::sqrt(static_cast<double>(squared))));
    bool due = true;
    u32 timer = role.soundTimer & Role::SoundPeriodMask;

    if (timer != Role::SoundPlaysOnce)
    {
        const u32 left = (role.soundTimer & Role::SoundCountMask) - 1;

        if (left != 0)
        {
            timer += left;
            due = false;
            role.soundTimer = timer;
        }
    }

    if (distance > kSoundRange || (role.flags & Role::Visible) == 0)
    {
        if ((role.flags & Role::SoundOnce) != 0)
        {
            effects.Stop(role.sound);
            role.flags &= ~Role::SoundOnce;
        }

        return;
    }

    u32 index = 0;
    state.FindRole(role.id, index);

    if ((role.flags & Role::SoundOnce) == 0 && due)
    {
        const u32 reloaded = (role.soundTimer >> Role::SoundPeriodShift) + (role.soundTimer & Role::SoundPeriodMask);
        role.soundTimer = reloaded;

        if ((reloaded & Role::SoundPeriodMask) == Role::SoundPlaysOnce)
        {
            role.flags |= Role::SoundOnce;
            effects.Play(role.sound, 0, 0, kPlayOnce);
        }
        else
        {
            effects.Play(role.sound, 0, 0, kPlayLooped);
        }

        if (index < view.soundDistance.size())
        {
            view.soundDistance[index] = static_cast<i16>(distance * kSoundScale / kSoundRange);
            const auto rise =
                static_cast<i16>(static_cast<u16>(static_cast<u16>(role.y) - static_cast<u16>(player.y)) * kPanScale);
            view.soundRise[index] = static_cast<i16>(rise / kSoundRange);
        }
    }

    effects.SetVolume(role.sound, kSoundScale - distance * kSoundScale / kSoundRange);
    effects.SetPan(role.sound, (role.x - player.x) * kPanScale / kSoundRange);
}

// swd3e.exe 0x00414BF0: the shadow of a role: its picture tinted onto the destination, zoomed about the act's anchor
// (16, 8 to the right and down) to the role's scale across and half of it down (a quarter from shadow kind 5 on),
// sheared to the right for odd kinds (to the left otherwise), flipped for kinds whose (kind - 1) % 4 is 2 or 3, and
// darkened by `tint`. The kind is the map's (Field::shadowKind) whatever `kind` is passed.
void FieldView::DrawShadow(const Role& role, const Picture& picture, int x, int y, int kind, int tint, int scale)
{
    static_cast<void>(kind);
    Painter& painter = this->game.painter;
    const int shadow = this->game.field.shadowKind;
    const u32 bits = role.act.flags;
    painter.SetSource(picture.data.data());
    painter.SetZoomSheared(
        role.act.x + kZoomAnchorX,
        role.act.y + kZoomAnchorY,
        scale,
        scale / (shadow < kQuarterShadowKind ? kHalfShadow : kQuarterShadow),
        (shadow & kShadowLeansRight) != 0 ? kShadowShear : -kShadowShear
    );
    DrawOptions& options = painter.Options();
    options.tintBlue = tint;
    options.tintGreen = tint;
    options.tintRed = tint;
    const u32 flip = (shadow - 1) % kShadowKinds < kFlippedShadowKinds ? 0u : static_cast<u32>(Painter::Flip);
    painter.DrawPicture(
        x,
        y,
        picture.width,
        picture.height,
        (bits & (Painter::RleSource | Painter::Mirror | Painter::Flip)) | Painter::ZoomTint | flip
    );
}

// swd3e.exe 0x00416450: the bubbles of role `role` near the camera: each mark of the role follows it, starts a new
// bubble now and then (at most while its count is below 0x18), and its bubbles rise, drift with the frame's sway and
// fade (warmer on map 0x32); spent ones are dropped by moving the next one into their place
void FieldView::DrawBubbles(int x, int y, u16 role)
{
    const Field& field = this->game.field;

    if (x <= field.cameraLeft - kBubbleMargin || x >= field.cameraRight + kBubbleMargin ||
        y <= field.cameraTop - kBubbleMargin || y >= field.cameraBottom + kBubbleMargin)
    {
        return;
    }

    Game& owner = this->game;
    Random& random = owner.random;
    Scene& view = owner.scene;
    Painter& painter = owner.painter;
    const bool warm = owner.currentMap == Field::MonasteryOutside;

    auto draw = [&](const RoleMarkNode& bubble, const Picture& picture)
    {
        DrawOptions& options = painter.Options();

        if (warm)
        {
            options.tintRed = bubble.life - kWarmRed;
            options.tintGreen = bubble.life - kWarmGreen;
            options.tintBlue = bubble.life - kWarmBlue;
        }
        else
        {
            options.tintBlue = (bubble.life >> 1) - kColdTint;
            options.tintGreen = (bubble.life >> 1) - kColdTint;
            options.tintRed = (bubble.life >> 1) - kColdTint;
        }

        const std::array<i32, 2> place =
            this->EffectAt(EffectKey(EffectKind::Bubbles, &bubble), bubble.x / kSubpixels, bubble.y, false);
        painter.SetSource(picture.data.data());
        painter.DrawPicture(
            place[0] - view.effectAct.x - field.cameraLeft,
            place[1] - view.effectAct.y - field.cameraTop,
            picture.width,
            picture.height,
            view.effectAct.flags
        );
    };

    if (this->replaying)
    {
        // Smooth motion: the bubbles as they are (the effect act is put back by PaintBetween).
        view.effectAct.id = Scene::EffectAct;
        view.effectAct.action = kBubbleAction;
        owner.acts.Run(view.effectAct);
        const Picture* shown = owner.archive.GetPicture(view.effectAct.frame, view.effectAct.picture);

        for (const RoleMark& mark : view.marks)
        {
            if (shown == nullptr || static_cast<i16>(mark.role) != static_cast<int>(role))
            {
                continue;
            }

            for (const RoleMarkNode* bubble = mark.nodes.get(); bubble != nullptr; bubble = bubble->next.get())
            {
                if (bubble->life > 0)
                {
                    draw(*bubble, *shown);
                }
            }
        }

        return;
    }

    const int sway = static_cast<int>(random.Below(kSwayRange)) - kSwayShift;
    view.effectAct.id = Scene::EffectAct;
    view.effectAct.action = kBubbleAction;
    owner.acts.Run(view.effectAct);
    const Picture* picture = owner.archive.GetPicture(view.effectAct.frame, view.effectAct.picture);

    for (RoleMark& mark : view.marks)
    {
        // 0x004164ED: the mark's role is sign-extended (movsx) against the zero-extended id, so ids from 0x8000 on
        // never match.
        if (static_cast<i16>(mark.role) != static_cast<int>(role))
        {
            continue;
        }

        u32 index = 0;
        owner.state.FindRoleOrPlayer(role, index);
        mark.flags &= ~RoleMark::Emitting;
        const Role& source = owner.state.roles[std::min<std::size_t>(index, GameState::Roles - 1)];
        mark.x = static_cast<u16>(source.x);
        mark.y = static_cast<u16>(source.y);

        if (random.Below(kPerMille) > kQuarterChance)
        {
            mark.flags |= RoleMark::Emitting;
        }

        if (mark.unk08 < kBubbleMarkLimit && (mark.flags & RoleMark::Emitting) != 0)
        {
            auto bubble = std::make_unique<RoleMarkNode>();
            bubble->next = std::move(mark.nodes);
            bubble->y = static_cast<i16>(mark.y);
            bubble->x = static_cast<i16>(
                kSubpixels * (mark.x - kBubbleSpread / 2 + static_cast<int>(random.Below(kBubbleSpread)))
            );
            bubble->life =
                static_cast<i16>(warm ? random.Below(kWarmLife) + kWarmLife : random.Below(kColdLife) + kColdLife);
            bubble->stepX = static_cast<i16>(static_cast<int>(random.Below(kBubbleSpread)) - kBubbleSpread / 2);
            bubble->stepY = static_cast<i16>(-1 - static_cast<int>(random.Below(1)));
            mark.nodes = std::move(bubble);
        }

        std::unique_ptr<RoleMarkNode>* link = &mark.nodes;

        while (*link != nullptr)
        {
            RoleMarkNode& bubble = **link;
            const u32 roll = random.Below(kPerMille);
            const i32 fromX = bubble.x / kSubpixels;
            const i32 fromY = bubble.y;
            bubble.y = static_cast<i16>(bubble.y + bubble.stepY);
            bubble.x = static_cast<i16>(bubble.x + sway + bubble.stepX);
            // The rebuild's continuous motion: the bubble takes its step over the next frame.
            this->MoveEffect(
                EffectKey(EffectKind::Bubbles, &bubble),
                fromX,
                fromY,
                bubble.x / kSubpixels,
                bubble.y,
                1,
                false,
                kBubbleSnap
            );

            if (roll > kBubbleAgeChance)
            {
                --bubble.life;
            }

            if (bubble.life > 0)
            {
                if (picture != nullptr)
                {
                    draw(bubble, *picture);
                }

                link = &bubble.next;
                continue;
            }

            if (bubble.next == nullptr)
            {
                link->reset();
                break;
            }

            std::unique_ptr<RoleMarkNode> following = std::move(bubble.next);
            bubble.x = following->x;
            bubble.y = following->y;
            bubble.stepX = following->stepX;
            bubble.stepY = following->stepY;
            bubble.life = following->life;
            bubble.next = std::move(following->next);
        }
    }
}

// swd3e.exe 0x004152A0: the role heads: drawn at (headX, targetY), a head with a speed slides out tripling it and is
// freed past the screen's sides, one without slides two thirds of the way to targetX each frame
void FieldView::DrawRoleHeads()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    std::unique_ptr<ScriptSprite>* link = &owner.scene.spritesB;

    while (*link != nullptr)
    {
        ScriptSprite& head = **link;
        const u64 key = EffectKey(EffectKind::RoleHeads, &head);
        const std::array<i32, 2> place = this->EffectAt(key, head.headX, head.targetY, true);
        const i32 fromX = head.headX;

        if (this->replaying)
        {
            // Smooth motion: the frame the act showed last where the head glides, nothing stepped, moved or freed.
            const Picture* shown = owner.archive.GetPicture(head.act.frame, head.act.picture);

            if (shown != nullptr)
            {
                painter.Options().alpha = head.act.signal;
                painter.SetSource(shown->data.data());
                painter.DrawPicture(
                    place[0] - head.act.x,
                    place[1] - head.act.y,
                    shown->width,
                    shown->height,
                    head.act.flags
                );
            }

            link = &head.next;
            continue;
        }

        if (owner.acts.Run(head.act) == 0)
        {
            ErrorBox(
                owner.errors,
                u8"Act Err(PicPaint:drawRoleHead)",
                u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
                head.act.id,
                head.act.action,
                head.act.direction
            );
        }

        const Picture* picture = owner.archive.GetPicture(head.act.frame, head.act.picture);

        if (picture != nullptr)
        {
            painter.Options().alpha = head.act.signal;
            painter.SetSource(picture->data.data());
            painter.DrawPicture(
                place[0] - head.act.x,
                place[1] - head.act.y,
                picture->width,
                picture->height,
                head.act.flags
            );
        }

        if ((head.headSpeed & ScriptSprite::HeadSpeedMask) != 0)
        {
            head.headX = static_cast<i16>(head.headX + head.headSpeed);
            head.headSpeed = static_cast<i16>(kHeadAcceleration * head.headSpeed);

            if (head.headX <= -ScriptSprite::HeadMargin || head.headX >= Display::Width + ScriptSprite::HeadMargin)
            {
                *link = std::move(head.next);
                continue;
            }

            // The rebuild's continuous motion: the head takes its slide over the next frame.
            this->MoveEffect(key, fromX, head.targetY, head.headX, head.targetY, 1, true, kHeadSnap);
            link = &head.next;
            continue;
        }

        const int slide = kSlideShare * (head.targetX - head.headX) / kSlideParts;
        head.headX = static_cast<i16>(head.headX + slide);

        if (slide <= 1 && slide >= -1)
        {
            head.headX = head.targetX;
        }

        this->MoveEffect(key, fromX, head.targetY, head.headX, head.targetY, 1, true, kHeadSnap);
        link = &head.next;
    }
}

}  // namespace swd3e
