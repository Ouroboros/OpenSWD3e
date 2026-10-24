#include <memory>

#include "audio/audio_manager.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/smooth/smooth_draw.h"
#include "config/config.h"
#include "core/error_box.h"
#include "draw/painter.h"
#include "game/game.h"
#include "pic/picture.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{

constexpr std::u8string_view kSecondTierTitle = u8"第二層";                 // 0x004BF54C
constexpr std::u8string_view kSecondTierAct = u8"第二層魔法act號碼錯誤!!";  // 0x004BF554
constexpr int kPanCentre = 0x140;
constexpr int kPan = 0x30;

}  // namespace

// swd3e.exe 0x0046AD80
void BattleState::ClearHitBits()
{
    this->hitBits = 0;
}

// swd3e.exe 0x0046AD90: a second-tier spell picture appended: act `id` with `action` at (x, y), mirrored when
// `mirrored` is 1 (an error box for an id of 0 or less)
HitPicture& BattleState::AddHitPicture(ErrorReporter& errors, i32 id, i32 action, i16 x, i16 y, i32 mirrored)
{
    if (id <= 0)
    {
        ErrorBox(errors, kSecondTierTitle, kSecondTierAct);
    }

    std::unique_ptr<HitPicture>* link = &this->hitPictures;

    while (*link != nullptr)
    {
        link = &(*link)->next;
    }

    *link = std::make_unique<HitPicture>();
    HitPicture& picture = **link;
    picture.act.action = action;
    picture.x = x;
    picture.y = y;
    picture.act.id = id;
    picture.mirrored = mirrored;
    return picture;
}

// swd3e.exe 0x0046AE10: one frame of the second-tier spell pictures: each act steps (a picture whose act cannot run
// stops the frame there, the waited ones kept) and is drawn at its place less its offset (mirrored as asked), its
// sound panned by its side; bit 5 of its target bits adds its effect as another picture (drawn this frame too), the
// others are gathered in hitBits (cleared first). Those whose act waited are freed.
void DrawHitPictures(BattleServices& services, smooth::Scene& scene)
{
    BattleState& battle = services.state;
    HitPicture* node = battle.hitPictures.get();

    if (node == nullptr)
    {
        return;
    }

    battle.ClearHitBits();

    if (services.acts.Run(node->act) == 0)
    {
        return;
    }

    smooth::ScenePainter painter(services, scene);

    while (true)
    {
        ActState& act = node->act;
        const std::unique_ptr<Picture> picture = services.archive.LoadPictureDirect(act.frame, act.picture);
        u32 flags = act.flags;
        int offsetX = act.x;
        const u16 width = picture != nullptr ? picture->width : u16{0};

        if (node->mirrored == 1)
        {
            flags ^= Painter::Mirror;

            if (offsetX != 0)
            {
                offsetX = width - offsetX;
            }
        }

        if ((act.target & ActState::AddsPicture) != 0)
        {
            if (act.effectA != 0)
            {
                battle.AddHitPicture(services.errors, act.effectA, act.effectQ, node->x, node->y, node->mirrored);
            }

            act.target = 0;
        }

        if (act.target != 0)
        {
            battle.hitBits = static_cast<u16>(battle.hitBits | act.target);
            act.target = 0;
        }

        services.audio.PlaySound(act.view, services.config.soundVolume);
        services.audio.SetSoundPan(act.view, node->x >= kPanCentre ? kPan : -kPan);
        act.view = 0;

        // A picture that cannot be loaded is drawn from a null pointer in the original.
        if (picture != nullptr)
        {
            painter.SetSource(*picture);
            painter.DrawPicture(node->x - offsetX, node->y - act.y, picture->width, picture->height, flags);
        }

        if (act.waiting == 1)
        {
            node->done = 1;
        }

        node = node->next.get();

        if (node == nullptr)
        {
            break;
        }

        if (services.acts.Run(node->act) == 0)
        {
            return;
        }
    }

    std::unique_ptr<HitPicture>* link = &battle.hitPictures;

    while (*link != nullptr)
    {
        if ((*link)->done == 1)
        {
            *link = std::move((*link)->next);
        }
        else
        {
            link = &(*link)->next;
        }
    }
}

// The same, drawn at once (or handed to the battle frame's scene).
void DrawHitPictures(BattleServices& services)
{
    smooth::Scene scene;
    DrawHitPictures(services, scene);
    scene.Show(services);
}

// swd3e.exe 0x0046AFE0
bool BattleState::HitPicturesDone() const
{
    return this->hitPictures == nullptr;
}

// swd3e.exe 0x0046AFF0
u16 BattleState::HitBits() const
{
    return this->hitBits;
}

}  // namespace swd3e
