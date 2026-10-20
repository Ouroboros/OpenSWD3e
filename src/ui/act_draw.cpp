#include "ui/act_draw.h"

#include "core/debug_log.h"
#include "core/error_box.h"
#include "draw/painter.h"
#include "pic/picture.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{

constexpr u32 kTranslucent = Painter::Translucent;
constexpr u32 kDecimal = 10;
constexpr int kLabelLeft = 0x18;

}  // namespace

ActDrawer::ActDrawer(ActArchive& acts, TswArchive& archive, Painter& painter, ErrorReporter& errors)
    : acts(acts),
      archive(archive),
      painter(painter),
      errors(errors)
{
}

// swd3e.exe 0x0040EF60: steps the script and draws its picture at (x, y) minus the script's anchor, with the
// translucency of 'SG'
void ActDrawer::Draw(ActState& state, int x, int y)
{
    if (this->acts.Run(state) == 0)
    {
        ErrorBox(
            this->errors,
            u8"Act Err(Misc::DrawActData)",
            u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
            state.id,
            state.action,
            state.direction
        );
        return;
    }

    const Picture* picture = this->archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return;
    }

    this->painter.SetSource(picture->data.data());
    this->painter.Options().alpha = state.signal;
    this->painter.DrawPicture(x - state.x, y - state.y, picture->width, picture->height, state.flags);
}

// swd3e.exe 0x00411B50: `value` in the digits of picture 0x2377 leftwards from `right` (each digit against the one
// after it, one pixel lower), then the label 0x2355 0x18 pixels before it, all translucent at the screen alpha
// (0x004E582C, `alpha` here; the original's third argument is unused). The original draws a picture it cannot load
// through a null pointer; nothing is drawn here (see deviations.md).
void ActDrawer::DrawCounter(int right, int y, u32 value, int alpha)
{
    int x = right;

    do
    {
        const Picture* digit = this->archive.GetPicture(ActDrawer::Digits, value % kDecimal);

        if (digit != nullptr)
        {
            x -= digit->width;
            this->painter.SetSource(digit->data.data());
            this->painter.Options().alpha = alpha;
            this->painter.DrawPicture(x, y + 1, digit->width, digit->height, kTranslucent);
        }

        value /= kDecimal;
    } while (value != 0);

    const Picture* label = this->archive.GetPicture(ActDrawer::CounterLabel, 0);

    if (label != nullptr)
    {
        this->painter.SetSource(label->data.data());
        this->painter.Options().alpha = alpha;
        this->painter.DrawPicture(x - kLabelLeft, y, label->width, label->height, kTranslucent);
    }
}

// swd3e.exe 0x00440830: translucent at `alpha`, keeping only the mirror bits of the script's mode
void ActDrawer::DrawGhost(ActState& state, int x, int y, int alpha, double alphaFraction)
{
    if (this->acts.Run(state) == 0)
    {
        ErrorBox(
            this->errors,
            u8"Act Err(Menu::DrawActGhost)",
            u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
            state.id,
            state.action,
            state.direction
        );  // 0x004B0D58
        return;
    }

    const Picture* picture = this->archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return;
    }

    this->painter.SetSource(picture->data.data());
    this->painter.Options().alpha = alpha;
    this->painter.Options().alphaFraction = alphaFraction;
    this->painter.DrawPicture(
        x - state.x,
        y - state.y,
        picture->width,
        picture->height,
        (state.flags & 0x80000003u) | kTranslucent
    );
}

// swd3e.exe 0x0040F030: `mode` with the mirror bits of the script's mode, at the translucency of 'SG'
void ActDrawer::DrawWithMode(ActState& state, int x, int y, u32 mode)
{
    if (this->acts.Run(state) == 0)
    {
        ErrorBox(
            this->errors,
            u8"Act Err(Misc::DrawActData)",
            u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
            state.id,
            state.action,
            state.direction
        );
        return;
    }

    const Picture* picture = this->archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return;
    }

    this->painter.SetSource(picture->data.data());
    this->painter.Options().alpha = state.signal;
    this->painter
        .DrawPicture(x - state.x, y - state.y, picture->width, picture->height, mode | (state.flags & 0x80000003u));
}

// swd3e.exe 0x0040EFF0: picture `picture` of `file` at (x, y), opaque
void ActDrawer::DrawPicture(u16 file, u16 picture, int x, int y)
{
    const Picture* source = this->archive.GetPicture(file, picture);

    if (source == nullptr)
    {
        return;
    }

    this->painter.SetSource(source->data.data());
    this->painter.DrawPicture(x, y, source->width, source->height, 0);
}

}  // namespace swd3e
