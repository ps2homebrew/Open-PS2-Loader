/*
  Copyright 2009-2010 volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.
*/

#include "include/opl.h"
#include "include/dialogs.h"
#include "include/gui.h"
#include "include/lang.h"
#include "include/pad.h"
#include "include/renderman.h"
#include "include/fntsys.h"
#include "include/themes.h"
#include "include/util.h"
#include "include/sound.h"

// UI spacing of the dialogues (pixels between consecutive items)
#define UI_SPACING_H      10
#define UI_SPACING_V      2
// spacer ui element width (simulates tab)
#define UI_SPACER_WIDTH   50
// minimal pixel width of spacer
#define UI_SPACER_MINIMAL 30
// length of breaking line in pixels
#define UI_BREAK_LEN      600
// scroll speed (delay in ms!) when in dialogs
#define DIA_SCROLL_SPEED  300
// scroll speed (delay in ms!) when setting int value
#define DIA_INT_SET_SPEED 100

static int screenWidth;
static int screenHeight;

// Utility stuff
#define KEYB_MODE   2
#define KEYB_WIDTH  12
#define KEYB_HEIGHT 4
#define KEYB_ITEMS  (KEYB_WIDTH * KEYB_HEIGHT)

static void diaDrawBoundingBox(int x, int y, int w, int h, int focus)
{
    u64 color = focus ? gTheme->selTextColor : gTheme->textColor;

    color |= GS_SETREG_RGBA(0, 0, 0, 0xFF);
    color &= gColFocus;

    rmDrawRect(x - 5, y, w + 10, h + 10, color);
}

int diaShowKeyb(char *text, int maxLen, int hide_text, const char *title)
{
    int i, j, len = strlen(text), selkeyb = 0, x, w;
    int selchar = 0, selcommand = -1;
    char c[2] = "\0\0", *mask_buffer;
    static const char keyb0[KEYB_ITEMS] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '`', ':'};

    static const char keyb1[KEYB_ITEMS] = {
        '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', '~', '.'};
    const char *keyb = keyb0;

    char *commands[KEYB_HEIGHT] = {_l(_STR_BACKSPACE), _l(_STR_SPACE), _l(_STR_ENTER), _l(_STR_MODE)};
    GSTEXTURE *cmdicons[KEYB_HEIGHT];
    cmdicons[0] = thmGetTexture(SQUARE_ICON);
    cmdicons[1] = thmGetTexture(TRIANGLE_ICON);
    cmdicons[2] = thmGetTexture(START_ICON);
    cmdicons[3] = thmGetTexture(SELECT_ICON);

    rmGetScreenExtents(&screenWidth, &screenHeight);

    if (hide_text) {
        if ((mask_buffer = malloc(maxLen)) != NULL) {
            memset(mask_buffer, '*', len);
            mask_buffer[len] = '\0';
        }
    } else {
        mask_buffer = NULL;
    }

    while (1) {
        readPads();

        rmStartFrame();
        guiDrawBGPlasma();
        rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

        // Title
        if (title != NULL) {
            fntRenderString(gTheme->fonts[0], 25, 20, ALIGN_NONE, 0, 0, title, gTheme->textColor);
            // separating line
            rmDrawLine(25, 38, 615, 38, gColWhite);
        }

        // Text
        fntRenderString(gTheme->fonts[0], 50, 120, ALIGN_NONE, 0, 0, hide_text ? mask_buffer : text, gTheme->textColor);

        // separating line for simpler orientation
        rmDrawLine(25, 138, 615, 138, gColWhite);

        for (j = 0; j < KEYB_HEIGHT; j++) {
            for (i = 0; i < KEYB_WIDTH; i++) {
                c[0] = keyb[i + j * KEYB_WIDTH];

                x = 50 + i * 31;
                w = fntRenderString(gTheme->fonts[0], x, 170 + 3 * UI_SPACING_H * j, ALIGN_NONE, 0, 0, c, gTheme->uiTextColor) - x;
                if ((i + j * KEYB_WIDTH) == selchar)
                    diaDrawBoundingBox(x, 170 + 3 * UI_SPACING_H * j, w, UI_SPACING_H, 0);
            }
        }

        // Commands
        for (i = 0; i < KEYB_HEIGHT; i++) {
            if (cmdicons[i]) {
                int w = (cmdicons[i]->Width * 20) / cmdicons[i]->Height;
                int h = 20;
                rmDrawPixmap(cmdicons[i], 436, 170 + 3 * UI_SPACING_H * i, ALIGN_NONE, w, h, SCALING_RATIO, gDefaultCol);
            }

            x = 477;
            w = fntRenderString(gTheme->fonts[0], x, 170 + 3 * UI_SPACING_H * i, ALIGN_NONE, 0, 0, commands[i], gTheme->uiTextColor) - x;
            if (i == selcommand)
                diaDrawBoundingBox(x, 170 + 3 * UI_SPACING_H * i, w, UI_SPACING_H, 0);
        }

        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_CANCEL, gTheme->fonts[0], 500, 417, gTheme->selTextColor);

        rmEndFrame();

        if (getKey(KEY_LEFT)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1) {
                if (selchar % KEYB_WIDTH)
                    selchar--;
                else {
                    selcommand = selchar / KEYB_WIDTH;
                    selchar = -1;
                }
            } else {
                selchar = (selcommand + 1) * KEYB_WIDTH - 1;
                selcommand = -1;
            }
        } else if (getKey(KEY_RIGHT)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1) {
                if ((selchar + 1) % KEYB_WIDTH)
                    selchar++;
                else {
                    selcommand = selchar / KEYB_WIDTH;
                    selchar = -1;
                }
            } else {
                selchar = selcommand * KEYB_WIDTH;
                selcommand = -1;
            }
        } else if (getKey(KEY_UP)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1)
                selchar = (selchar + KEYB_ITEMS - KEYB_WIDTH) % KEYB_ITEMS;
            else
                selcommand = (selcommand + KEYB_HEIGHT - 1) % KEYB_HEIGHT;
        } else if (getKey(KEY_DOWN)) {
            sfxPlay(SFX_CURSOR);
            if (selchar > -1)
                selchar = (selchar + KEYB_WIDTH) % KEYB_ITEMS;
            else
                selcommand = (selcommand + 1) % KEYB_HEIGHT;
        } else if (getKeyOn(gSelectButton)) {
            if (len < (maxLen - 1) && selchar > -1) {
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL) {
                    mask_buffer[len] = '*';
                    mask_buffer[len + 1] = '\0';
                }

                len++;
                c[0] = keyb[selchar];
                strcat(text, c);
            } else if (selcommand == 0) {
                sfxPlay(SFX_CANCEL);
                if (len > 0) { // BACKSPACE
                    len--;
                    text[len] = 0;
                    if (mask_buffer != NULL)
                        mask_buffer[len] = '\0';
                }
            } else if (selcommand == 1) {
                sfxPlay(SFX_CONFIRM);
                if (len < (maxLen - 1)) { // SPACE
                    if (mask_buffer != NULL) {
                        mask_buffer[len] = '*';
                        mask_buffer[len + 1] = '\0';
                    }

                    len++;
                    c[0] = ' ';
                    strcat(text, c);
                }
            } else if (selcommand == 2) {
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL)
                    free(mask_buffer);
                return 1; // ENTER
            } else if (selcommand == 3) {
                sfxPlay(SFX_CONFIRM);
                selkeyb = (selkeyb + 1) % KEYB_MODE; // MODE
                if (selkeyb == 0)
                    keyb = keyb0;
                if (selkeyb == 1)
                    keyb = keyb1;
            }
        } else if (getKey(KEY_SQUARE)) {
            if (len > 0) { // BACKSPACE
                sfxPlay(SFX_CANCEL);
                len--;
                text[len] = 0;
                if (mask_buffer != NULL)
                    mask_buffer[len] = '\0';
            }
        } else if (getKey(KEY_TRIANGLE)) {
            if (len < (maxLen - 1) && selchar > -1) { // SPACE
                sfxPlay(SFX_CONFIRM);
                if (mask_buffer != NULL) {
                    mask_buffer[len] = '*';
                    mask_buffer[len + 1] = '\0';
                }

                len++;
                c[0] = ' ';
                strcat(text, c);
            }
        } else if (getKeyOn(KEY_START)) {
            sfxPlay(SFX_CONFIRM);
            if (mask_buffer != NULL)
                free(mask_buffer);
            return 1; // ENTER
        } else if (getKeyOn(KEY_SELECT)) {
            selkeyb = (selkeyb + 1) % KEYB_MODE; // MODE
            sfxPlay(SFX_CONFIRM);
            if (selkeyb == 0)
                keyb = keyb0;
            if (selkeyb == 1)
                keyb = keyb1;
        }

        if (getKey(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            sfxPlay(SFX_CANCEL);
            break;
        }
    }


    if (mask_buffer != NULL)
        free(mask_buffer);

    return 0;
}

static int colPadSettings[16];

static int diaShowColSel(unsigned char *r, unsigned char *g, unsigned char *b)
{
    int selc = 0;
    int ret = 0;
    unsigned char col[3];

    padStoreSettings(colPadSettings);

    col[0] = *r;
    col[1] = *g;
    col[2] = *b;
    setButtonDelay(KEY_LEFT, 1);
    setButtonDelay(KEY_RIGHT, 1);

    while (1) {
        readPads();

        rmStartFrame();
        guiDrawBGPlasma();
        rmDrawRect(0, 0, screenWidth, screenHeight, gColDarker);

        // "Color selection"
        fntRenderString(gTheme->fonts[0], 50, 50, ALIGN_NONE, 0, 0, _l(_STR_COLOR_SELECTION), GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));

        // 3 bars representing the colors...
        size_t co;
        int x, y;

        for (co = 0; co < 3; ++co) {
            unsigned char cc[3] = {0, 0, 0};
            cc[co] = col[co];

            x = 75;
            y = 75 + co * 25;

            u64 dcol = GS_SETREG_RGBA(cc[0], cc[1], cc[2], 0x80);

            if (selc == co)
                rmDrawRect(x, y, 200, 20, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
            else
                rmDrawRect(x, y, 200, 20, GS_SETREG_RGBA(0x20, 0x20, 0x20, 0x80));

            rmDrawRect(x + 2, y + 2, 190.0f * (cc[co] * 100 / 255) / 100, 16, dcol);
        }

        // target color itself
        u64 dcol = GS_SETREG_RGBA(col[0], col[1], col[2], 0x80);

        x = 300;
        y = 75;

        rmDrawRect(x, y, 70, 70, GS_SETREG_RGBA(0x60, 0x60, 0x60, 0x80));
        rmDrawRect(x + 5, y + 5, 60, 60, dcol);

        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CIRCLE_ICON : CROSS_ICON, _STR_OK, gTheme->fonts[0], 420, 417, gTheme->selTextColor);
        guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? CROSS_ICON : CIRCLE_ICON, _STR_CANCEL, gTheme->fonts[0], 500, 417, gTheme->selTextColor);

        rmEndFrame();

        if (getKey(KEY_LEFT)) {
            if (col[selc] > 0) {
                col[selc]--;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_RIGHT)) {
            if (col[selc] < 255) {
                col[selc]++;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_UP)) {
            if (selc > 0) {
                selc--;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKey(KEY_DOWN)) {
            if (selc < 2) {
                selc++;
                sfxPlay(SFX_CURSOR);
            }
        } else if (getKeyOn(gSelectButton)) {
            sfxPlay(SFX_CONFIRM);
            *r = col[0];
            *g = col[1];
            *b = col[2];
            ret = 1;
            break;
        } else if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
            sfxPlay(SFX_CANCEL);
            ret = 0;
            break;
        }
    }

    padRestoreSettings(colPadSettings);
    return ret;
}



// ----------------------------------------------------------------------------
// --------------------------- Dialogue handling ------------------------------
// ----------------------------------------------------------------------------
static const char *diaGetLocalisedText(const char *def, int id)
{
    if (id >= 0)
        return _l(id);

    return def;
}

/// returns true if the item is controllable (e.g. a value can be changed on it)
static int diaIsControllable(struct UIItem *ui)
{
    return (ui->enabled && ui->visible && (ui->type >= UI_OK));
}

/// returns true if the given item should be preceded with nextline
static int diaShouldBreakLine(struct UIItem *ui)
{
    return (ui->type == UI_SPLITTER || ui->type == UI_OK || ui->type == UI_BREAK);
}

/// returns true if the given item should be superseded with nextline
static int diaShouldBreakLineAfter(struct UIItem *ui)
{
    return (ui->type == UI_SPLITTER);
}

static void diaDrawHint(int text_id)
{
    int x, y;
    char *text = _l(text_id);

    x = screenWidth - rmUnScaleX(fntCalcDimensions(gTheme->fonts[0], text)) - 10;
    y = gTheme->usedHeight - 62;

    // render hint on the lower side of the screen.
    rmDrawRect(x, y, screenWidth - x, MENU_ITEM_HEIGHT + 10, gColDarker);
    fntRenderString(gTheme->fonts[0], x + 5, y + 5, ALIGN_NONE, 0, 0, text, gTheme->textColor);
}

/// renders an ui item (either selected or not)
/// sets width and height of the render into the parameters
static void diaRenderItem(int x, int y, struct UIItem *item, int selected, int haveFocus, int *w, int *h)
{
    // Don't draw controllable items that are not visible.
    if (!item->visible && item->type >= UI_LABEL)
        return;

    *h = UI_SPACING_H;

    // all texts are rendered up from the given point!
    u64 txtcol;

    if (diaIsControllable(item))
        txtcol = gTheme->uiTextColor;
    else
        txtcol = gTheme->textColor;

    // let's see what do we have here?
    switch (item->type) {
        case UI_TERMINATOR:
            return;

        case UI_BUTTON:
        case UI_LABEL: {
            // width is text length in pixels...
            const char *txt = diaGetLocalisedText(item->label.text, item->label.stringId);
            if (txt && strlen(txt))
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, txt, txtcol) - x;
            else
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, _l(_STR_NOT_SET), txtcol) - x;

            break;
        }

        case UI_SPLITTER: {
            // a line. Thanks to the font rendering, we need to shift up by one font line
            *w = 0;                          // nothing to render at all
            int ypos = y - UI_SPACING_V / 2; //  gsFont->CharHeight +

            // to ODD lines
            ypos &= ~1;

            rmDrawLine(x, ypos, x + UI_BREAK_LEN, ypos, gColWhite);
            break;
        }

        case UI_BREAK:
            *w = 0; // nothing to render at all
            break;

        case UI_SPACER: {
            // next column divisible by spacer
            *w = (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);

            if (*w < UI_SPACER_MINIMAL) {
                x += *w + UI_SPACER_MINIMAL;
                *w += (UI_SPACER_WIDTH - x % UI_SPACER_WIDTH);
            }

            *h = 0;
            break;
        }

        case UI_OK: {
            const char *txt = _l(_STR_OK);
            *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, txt, txtcol) - x;
            break;
        }

        case UI_INT: {
            char tmp[10];

            snprintf(tmp, sizeof(tmp), "%d", item->intvalue.current);
            *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, tmp, txtcol) - x;
            break;
        }

        case UI_STRING: {
            if (strlen(item->stringvalue.text))
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, item->stringvalue.text, txtcol) - x;
            else
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, _l(_STR_NOT_SET), txtcol) - x;
            break;
        }

        case UI_PASSWORD: {
            char stars[32];
            int i;
            int len;

            if (strlen(item->stringvalue.text)) {
                len = min(strlen(item->stringvalue.text), sizeof(stars) - 1);
                for (i = 0; i < len; ++i)
                    stars[i] = '*';

                stars[i] = '\0';
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, stars, txtcol) - x;
            } else
                *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, _l(_STR_NOT_SET), txtcol) - x;
            break;
        }

        case UI_BOOL: {
            const char *txtval = _l((item->intvalue.current) ? _STR_ON : _STR_OFF);
            *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, txtval, txtcol) - x;
            break;
        }

        case UI_ENUM: {
            const char *tv = item->intvalue.enumvalues[item->intvalue.current];

            if (!tv)
                tv = _l(_STR_NO_ITEMS);

            *w = fntRenderString(gTheme->fonts[0], x, y, ALIGN_NONE, 0, 0, tv, txtcol) - x;
            break;
        }

        case UI_COLOUR: {
            *w = rmWideScale(25);
            *h = 17;

            // Align to the right
            x -= *w;

            rmDrawRect(x, y + 3, *w, *h, txtcol);
            u64 dcol = GS_SETREG_RGBA(item->colourvalue.r, item->colourvalue.g, item->colourvalue.b, 0x80);
            rmDrawRect(x + 2, y + 5, *w - 4, *h - 4, dcol);

            break;
        }
    }

    if (selected)
        diaDrawBoundingBox(x, y, *w, *h, haveFocus);

    if (item->fixedWidth != 0) {
        int newSize;
        if (item->fixedWidth < 0)
            newSize = item->fixedWidth * screenWidth / -100;
        else
            newSize = item->fixedWidth;
        if (*w < newSize)
            *w = newSize;
    }

    if (item->fixedHeight != 0) {
        int newSize;
        if (item->fixedHeight < 0)
            newSize = item->fixedHeight * screenHeight / -100;
        else
            newSize = item->fixedHeight;
        if (*h < newSize)
            *h = newSize;
    }
}

/// renders whole ui screen (for given dialog setup)
void diaRenderUI(struct UIItem *ui, short inMenu, struct UIItem *cur, int haveFocus)
{
    guiDrawBGPlasma();

    int x0 = 20;
    int y0 = 20;

    // render all items
    struct UIItem *rc = ui;
    int x = x0, y = y0, hmax = 0;

    while (rc->type != UI_TERMINATOR) {
        int w = 0, h = 0;

        if (diaShouldBreakLine(rc)) {
            x = x0;

            if (hmax > 0)
                y += hmax + UI_SPACING_H;

            hmax = 0;
        }

        diaRenderItem(x, y, rc, rc == cur, haveFocus, &w, &h);

        if (w > 0)
            x += w + UI_SPACING_V;

        hmax = (h > hmax) ? h : hmax;

        if (diaShouldBreakLineAfter(rc)) {
            x = x0;

            if (hmax > 0)
                y += hmax + UI_SPACING_H;

            hmax = 0;
        }

        rc++;
    }

    if ((cur != NULL) && (!haveFocus) && (cur->hintId != -1)) {
        diaDrawHint(cur->hintId);
    }

    int uiHints[2] = {_STR_SELECT, _STR_BACK};
    int uiIcons[2] = {CIRCLE_ICON, CROSS_ICON};
    int uiY = gTheme->usedHeight - 32;
    int uiX = guiAlignSubMenuHints(2, uiHints, uiIcons, gTheme->fonts[0], 12, 2);

    uiX = guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? uiIcons[0] : uiIcons[1], uiHints[0], gTheme->fonts[0], uiX, uiY, gTheme->textColor);
    uiX += 12;
    uiX = guiDrawIconAndText(gSelectButton == KEY_CIRCLE ? uiIcons[1] : uiIcons[0], uiHints[1], gTheme->fonts[0], uiX, uiY, gTheme->textColor);
}

/// sets the ui item value to the default again
static void diaResetValue(struct UIItem *item)
{
    switch (item->type) {
        case UI_INT:
        case UI_BOOL:
            item->intvalue.current = item->intvalue.def;
            return;
        case UI_STRING:
        case UI_PASSWORD:
            strncpy(item->stringvalue.text, item->stringvalue.def, sizeof(item->stringvalue.text));
            return;
        default:
            return;
    }
}

static int diaHandleInput(struct UIItem *item, int *modified)
{
    // circle loses focus, sets old values first
    if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
        diaResetValue(item);
        sfxPlay(SFX_CONFIRM);
        return 0;
    }

    // cross loses focus without setting default
    if (getKeyOn(gSelectButton)) {
        sfxPlay(SFX_CONFIRM);
        *modified = 0;
        return 0;
    }

    // UI item type dependant part:
    if (item->type == UI_BOOL) {
        // a trick. Set new value, lose focus
        item->intvalue.current = !item->intvalue.current;
        return 0;
    }
    if (item->type == UI_INT) {
        // to be sure
        setButtonDelay(KEY_UP, DIA_INT_SET_SPEED);
        setButtonDelay(KEY_DOWN, DIA_INT_SET_SPEED);

        // up and down
        if (getKey(KEY_UP)) {
            sfxPlay(SFX_CURSOR);
            if (item->intvalue.current < item->intvalue.max) {
                item->intvalue.current++;
            } else {
                item->intvalue.current = item->intvalue.min; // was "= 0;"
            }
        } else if (getKey(KEY_DOWN)) {
            sfxPlay(SFX_CURSOR);
            if (item->intvalue.current > item->intvalue.min) {
                item->intvalue.current--;
            } else {
                item->intvalue.current = item->intvalue.max;
            }
        } else
            *modified = 0;
    } else if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        char tmp[32];
        strncpy(tmp, item->stringvalue.text, sizeof(tmp));

        if (item->stringvalue.handler) {
            if (item->stringvalue.handler(tmp, sizeof(tmp)))
                strncpy(item->stringvalue.text, tmp, sizeof(item->stringvalue.text));
        } else {
            if (diaShowKeyb(tmp, sizeof(tmp), item->type == UI_PASSWORD, NULL))
                strncpy(item->stringvalue.text, tmp, sizeof(item->stringvalue.text));
        }

        return 0;
    } else if (item->type == UI_ENUM) {
        int cur = item->intvalue.current;

        if (item->intvalue.enumvalues[cur] == NULL) {
            if (cur > 0)
                item->intvalue.current--;
            else
                return 0;
        }

        if (getKey(KEY_UP) && (item->intvalue.current > 0)) {
            item->intvalue.current--;
            sfxPlay(SFX_CURSOR);
        } else if (getKey(KEY_DOWN) && (item->intvalue.enumvalues[item->intvalue.current + 1] != NULL)) {
            item->intvalue.current++;
            sfxPlay(SFX_CURSOR);
        }

        else {
            *modified = 0;
        }

    } else if (item->type == UI_COLOUR) {
        if (!diaShowColSel(&item->colourvalue.r, &item->colourvalue.g, &item->colourvalue.b))
            *modified = 0;

        return 0;
    }

    return 1;
}

static struct UIItem *diaGetFirstControl(struct UIItem *ui)
{
    struct UIItem *cur = ui;

    while (!diaIsControllable(cur)) {
        if (cur->type == UI_TERMINATOR)
            return ui;

        cur++;
    }

    return cur;
}

static struct UIItem *diaGetLastControl(struct UIItem *ui)
{
    struct UIItem *last = diaGetFirstControl(ui);
    struct UIItem *cur = last;

    while (cur->type != UI_TERMINATOR) {
        cur++;

        if (diaIsControllable(cur))
            last = cur;
    }

    return last;
}

static struct UIItem *diaGetNextControl(struct UIItem *cur, struct UIItem *dflt)
{
    while (cur->type != UI_TERMINATOR) {
        cur++;

        if (diaIsControllable(cur))
            return cur;
    }

    return dflt;
}

static struct UIItem *diaGetPrevControl(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    while (newf != ui) {
        newf--;

        if (diaIsControllable(newf))
            return newf;
    }

    return cur;
}

/// finds first control on previous line...
static struct UIItem *diaGetPrevLine(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    int lb = 0;
    int hadCtrl = 0; // had the scanned line any control?

    while (newf != ui) {
        newf--;

        if ((lb > 0) && (diaIsControllable(newf)))
            hadCtrl = 1;

        if (diaShouldBreakLine(newf)) { // is this a line break?
            if (hadCtrl || lb == 0) {
                lb++;
                hadCtrl = 0;
            }
        }

        // twice the break? find first control
        if (lb == 2)
            return diaGetFirstControl(newf);
    }

    return cur;
}

static struct UIItem *diaGetNextLine(struct UIItem *cur, struct UIItem *ui)
{
    struct UIItem *newf = cur;

    int lb = 0;

    while (newf->type != UI_TERMINATOR) {
        newf++;

        if (diaShouldBreakLine(newf)) { // is this a line break?
            lb++;
        }

        if (lb == 1)
            return diaGetNextControl(newf, cur);
    }

    return cur;
}

static int diaPadSettings[16];

static void diaStoreScrollSpeed(void)
{
    padStoreSettings(diaPadSettings);
}

static void diaRestoreScrollSpeed(void)
{
    padRestoreSettings(diaPadSettings);
}

static struct UIItem *diaFindByID(struct UIItem *ui, int id)
{
    while (ui->type != UI_TERMINATOR) {
        if (ui->id == id)
            return ui;

        ui++;
    }

    return NULL;
}

int diaExecuteDialog(struct UIItem *ui, int uiId, short inMenu, int (*updater)(int modified))
{
    rmGetScreenExtents(&screenWidth, &screenHeight);

    struct UIItem *cur = NULL;
    if (uiId != -1)
        cur = diaFindByID(ui, uiId);

    if (!cur)
        cur = diaGetFirstControl(ui);

    // what? no controllable item? Exit!
    if (cur == ui)
        return -1;

    int haveFocus = 0, modified;

    diaStoreScrollSpeed();

    // slower controls for dialogs
    setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
    setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);

    // okay, we have the first selectable item
    // we can proceed with rendering etc. etc.
    while (1) {
        rmStartFrame();
        diaRenderUI(ui, inMenu, cur, haveFocus);
        rmEndFrame();

        readPads();

        if (haveFocus) {
            modified = 1;
            haveFocus = diaHandleInput(cur, &modified);

            if (!haveFocus) {
                setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
                setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);
            }
        } else {
            modified = 0;
            struct UIItem *newf = cur;

            if (getKey(KEY_LEFT)) {
                newf = diaGetPrevControl(cur, ui);
                if (newf == cur)
                    newf = diaGetLastControl(ui);
            }

            if (getKey(KEY_RIGHT)) {
                newf = diaGetNextControl(cur, cur);
                if (newf == cur)
                    newf = diaGetFirstControl(ui);
            }

            if (getKey(KEY_UP)) {
                newf = diaGetPrevLine(cur, ui);
                if (newf == cur)
                    newf = diaGetLastControl(ui);
            }

            if (getKey(KEY_DOWN)) {
                newf = diaGetNextLine(cur, ui);
                if (newf == cur)
                    newf = diaGetFirstControl(ui);
            }

            if (newf != cur) {
                // Navigation change detected
                sfxPlay(SFX_CURSOR);
                cur = newf;
            }

            // Cancel button breaks focus or exits with false result
            if (getKeyOn(gSelectButton == KEY_CIRCLE ? KEY_CROSS : KEY_CIRCLE)) {
                diaRestoreScrollSpeed();
                sfxPlay(SFX_CANCEL);
                return UIID_BTN_CANCEL;
            }

            // see what key events we have
            if (getKeyOn(gSelectButton)) {
                haveFocus = 1;
                sfxPlay(SFX_CONFIRM);

                if (cur->type == UI_BUTTON) {
                    diaRestoreScrollSpeed();
                    return cur->id;
                }

                if (cur->type == UI_OK) {
                    diaRestoreScrollSpeed();
                    return UIID_BTN_OK;
                }
            }
        }

        if (updater) {
            int updResult = updater(modified);
            if (updResult)
                return updResult;
        }
    }
}

void diaSetEnabled(struct UIItem *ui, int id, int enabled)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->enabled = enabled;
}

void diaSetVisible(struct UIItem *ui, int id, int visible)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->visible = visible;
}

void diaSetItemType(struct UIItem *ui, int id, UIItemType type)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return;

    item->type = type;
}

int diaGetInt(struct UIItem *ui, int id, int *value)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_INT) || (item->type == UI_BOOL) || (item->type == UI_ENUM)) {
        *value = item->intvalue.current;
        return 1;
    }

    return 0;
}

int diaSetInt(struct UIItem *ui, int id, int value)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_INT) || (item->type == UI_BOOL) || (item->type == UI_ENUM)) {
        item->intvalue.def = value;
        item->intvalue.current = value;
        return 1;
    }

    return 0;
}

int diaGetString(struct UIItem *ui, int id, char *value, int length)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        strncpy(value, item->stringvalue.text, length);
        return 1;
    }

    return 0;
}

int diaSetString(struct UIItem *ui, int id, const char *text)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_STRING) || (item->type == UI_PASSWORD)) {
        strncpy(item->stringvalue.def, text, sizeof(item->stringvalue.def));
        item->stringvalue.def[sizeof(item->stringvalue.def) - 1] = '\0';
        strncpy(item->stringvalue.text, text, sizeof(item->stringvalue.text));
        item->stringvalue.text[sizeof(item->stringvalue.text) - 1] = '\0';
        return 1;
    }

    return 0;
}

int diaGetColor(struct UIItem *ui, int id, unsigned char *col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    col[0] = item->colourvalue.r;
    col[1] = item->colourvalue.g;
    col[2] = item->colourvalue.b;
    return 1;
}

int diaSetColor(struct UIItem *ui, int id, const unsigned char *col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    item->colourvalue.r = col[0];
    item->colourvalue.g = col[1];
    item->colourvalue.b = col[2];
    return 1;
}

int diaSetU64Color(struct UIItem *ui, int id, u64 col)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_COLOUR)
        return 0;

    item->colourvalue.r = col & 0xFF;
    col >>= 8;
    item->colourvalue.g = col & 0xFF;
    col >>= 8;
    item->colourvalue.b = col & 0xFF;
    return 1;
}

int diaSetLabel(struct UIItem *ui, int id, const char *text)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if ((item->type == UI_LABEL) || (item->type == UI_BUTTON)) {
        item->label.text = text;
        return 1;
    }

    return 0;
}

int diaSetEnum(struct UIItem *ui, int id, const char **enumvals)
{
    struct UIItem *item = diaFindByID(ui, id);

    if (!item)
        return 0;

    if (item->type != UI_ENUM)
        return 0;

    item->intvalue.enumvalues = enumvals;
    return 1;
}
