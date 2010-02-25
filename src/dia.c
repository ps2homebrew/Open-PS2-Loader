/*
  Copyright 2009-2010 volca
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/dia.h"
#include "include/lang.h"
#include "include/pad.h"
#include "include/gfx.h"

// Row height in dialogues
#define UI_ROW_HEIGHT 10
// UI spacing of the dialogues (pixels between consecutive items)
#define UI_SPACING_H 10
#define UI_SPACING_V 2
// spacer ui element width (simulates tab)
#define UI_SPACER_WIDTH 50
// minimal pixel width of spacer
#define UI_SPACER_MINIMAL 30
// length of breaking line in pixels
#define UI_BREAK_LEN 600
// scroll speed when in dialogs
#define DIA_SCROLL_SPEED 9

// ----------------------------------------------------------------------------
// --------------------------- Dialogue handling ------------------------------
// ----------------------------------------------------------------------------
const char *diaGetLocalisedText(const char* def, int id) {
	if (id >= 0)
		return _l(id);
	
	return def;
}

/// returns true if the item is controllable (e.g. a value can be changed on it)
int diaIsControllable(struct UIItem *ui) {
	return (ui->type >= UI_OK);
}

/// returns true if the given item should be preceded with nextline
int diaShouldBreakLine(struct UIItem *ui) {
	return (ui->type == UI_SPLITTER || ui->type == UI_OK || ui->type == UI_BREAK);
}

/// returns true if the given item should be superseded with nextline
int diaShouldBreakLineAfter(struct UIItem *ui) {
	return (ui->type == UI_SPLITTER);
}

/// renders an ui item (either selected or not)
/// sets width and height of the render into the parameters
void diaRenderItem(int x, int y, struct UIItem *item, int selected, int haveFocus, int *w, int *h) {
	// height fixed for now
	*h = UI_ROW_HEIGHT;
	
	// all texts are rendered up from the given point!
	
	if (selected) {
		if (haveFocus) // a slightly different color for focus instead of selection
			textColor(0xff, 0x080, 0x00, 0xff);
		else
			textColor(0x00, 0x0ff, 0x00, 0xff);
			
	} else {
		if (diaIsControllable(item))
			textColor(0x00, 0x0ff, 0x80, 0xff);
		else
			textColor(text_color[0], text_color[1], text_color[2], 0xff);
	}
	
	// let's see what do we have here?
	switch (item->type) {
		case UI_TERMINATOR:
			return;
		
		case UI_BUTTON:	
		case UI_LABEL: {
				// width is text lenght in pixels...
				const char *txt = diaGetLocalisedText(item->label.text, item->label.stringId);
				*w = strlen(txt) * gsFont->CharWidth;
			
				drawText(x, y, txt, 1.0f, 0);
				break;
			}
		case UI_SPLITTER: {
				// a line. Thanks to the font rendering, we need to shift up by one font line
				*w = 0; // nothing to render at all
				*h = UI_SPACING_H;
				int ypos = y - UI_SPACING_V / 2; //  gsFont->CharHeight +
				// two lines for lesser eye strain :)
				drawLine(gsGlobal, x, ypos, x + UI_BREAK_LEN, ypos, 1, gColWhite);
				drawLine(gsGlobal, x, ypos + 1, x + UI_BREAK_LEN, ypos + 1, 1, gColWhite);
				break;
			}
		case UI_BREAK:
			*w = 0; // nothing to render at all
			*h = 0;
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
				*w = strlen(txt) * gsFont->CharWidth;
				*h = gsFont->CharHeight;
				
				drawText(x, y, txt, 1.0f, 0);
				break;
			}

		case UI_INT: {
				char tmp[10];
				
				snprintf(tmp, 10, "%d", item->intvalue.current);
				*w = strlen(tmp) * gsFont->CharWidth;
				
				drawText(x, y, tmp, 1.0f, 0);
				break;
			}
		case UI_STRING: {
				*w = strlen(item->stringvalue.text) * gsFont->CharWidth;
				
				drawText(x, y, item->stringvalue.text, 1.0f, 0);
				break;
			}
		case UI_BOOL: {
				const char *txtval = _l((item->intvalue.current) ? _STR_ON : _STR_OFF);
				*w = strlen(txtval) * gsFont->CharWidth;
				
				drawText(x, y, txtval, 1.0f, 0);
				break;
			}
		case UI_ENUM: {
				const char* tv = item->intvalue.enumvalues[item->intvalue.current];

				if (!tv)
					tv = "<no value>";

				*w = strlen(tv) * gsFont->CharWidth;
				
				drawText(x, y, tv, 1.0f, 0);
				break;
		}
		case UI_COLOUR: {
				u64 dcol = GS_SETREG_RGBAQ(item->colourvalue.r,item->colourvalue.g,item->colourvalue.b, 0x00, 0x00);
				if (selected)
					drawQuad(gsGlobal, x, y, x + 25, y, x, y + 15, x+25, y + 15, gZ, gColWhite);
				else
					drawQuad(gsGlobal, x, y, x + 25, y, x, y + 15, x+25, y + 15, gZ, gColDarker);

				drawQuad(gsGlobal, x + 2, y + 2, x + 23, y + 2, x + 2, y + 13, x+23, y + 13, gZ, dcol);
				*w = 15;
				break;
		}
	}
}

/// renders whole ui screen (for given dialog setup)
void diaRenderUI(struct UIItem *ui, struct UIItem *cur, int haveFocus) {
	// clear screen, draw background
	drawScreen();
	
	// darken for better contrast
	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0,1,0,1,0), 0);
	drawQuad(gsGlobal, 0.0f, 0.0f, 640.0f, 0.0f, 0.0f, 512.0f, 640.0f, 512.0f, gZ, gColDarker);
	gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);


	// TODO: Sanitize these values
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

	if ((cur != NULL) && (!haveFocus) && (cur->hint != NULL)) {
		drawHint(cur->hint, -1);
	}
	
	// flip display
	flip();
}

/// sets the ui item value to the default again
void diaResetValue(struct UIItem *item) {
	switch(item->type) {
		case UI_INT:
		case UI_BOOL:
			item->intvalue.current = item->intvalue.def;
			return;
		case UI_STRING:
			strncpy(item->stringvalue.text, item->stringvalue.def, 32);
			return;
		default:
			return;
	}
}

int diaHandleInput(struct UIItem *item) {
	// circle loses focus, sets old values first
	if (getKeyOn(KEY_CIRCLE)) {
		diaResetValue(item);
		return 0;
	}
	
	// cross loses focus without setting default
	if (getKeyOn(KEY_CROSS))
		return 0;
	
	// UI item type dependant part:
	if (item->type == UI_BOOL) {
		// a trick. Set new value, lose focus
		item->intvalue.current = !item->intvalue.current;
		return 0;
	} if (item->type == UI_INT) {
		// to be sure
		setButtonDelay(KEY_UP, 1);
		setButtonDelay(KEY_DOWN, 1);

		// up and down
		if (getKey(KEY_UP) && (item->intvalue.current < item->intvalue.max))
			item->intvalue.current++;
		
		if (getKey(KEY_DOWN) && (item->intvalue.current > item->intvalue.min))
			item->intvalue.current--;
		
	} else if (item->type == UI_STRING) {
		char tmp[32];
		strncpy(tmp, item->stringvalue.text, 32);

		if (showKeyb(tmp, 32))
			strncpy(item->stringvalue.text, tmp, 32);

		return 0;
	} else if (item->type == UI_ENUM) {
		int cur = item->intvalue.current;

		if (item->intvalue.enumvalues[cur] == NULL) {
			if (cur > 0)
				item->intvalue.current--;
			else
				return 0;
		}

		if (getKey(KEY_UP) && (cur > 0))
			item->intvalue.current--;
		
		++cur;

		if (getKey(KEY_DOWN) && (item->intvalue.enumvalues[cur] != NULL))
			item->intvalue.current = cur;
	} else if (item->type == UI_COLOUR) {
		unsigned char col[3];

		col[0] = item->colourvalue.r;
		col[1] = item->colourvalue.g;
		col[2] = item->colourvalue.b;

		if (showColSel(&col[0], &col[1], &col[2])) {
			item->colourvalue.r = col[0];
			item->colourvalue.g = col[1];
			item->colourvalue.b = col[2];
		}

		return 0;
	}
	
	return 1;
}

struct UIItem *diaGetFirstControl(struct UIItem *ui) {
	struct UIItem *cur = ui;
	
	while (!diaIsControllable(cur)) {
		if (cur->type == UI_TERMINATOR)
			return ui; 
		
		cur++;
	}
	
	return cur;
}

struct UIItem *diaGetLastControl(struct UIItem *ui) {
	struct UIItem *last = diaGetFirstControl(ui);
	struct UIItem *cur = last;
	
	while (cur->type != UI_TERMINATOR) {
		cur++;
		
		if (diaIsControllable(cur))
			last = cur;
	}
	
	return last;
}

struct UIItem *diaGetNextControl(struct UIItem *cur, struct UIItem* dflt) {
	while (cur->type != UI_TERMINATOR) {
		cur++;
		
		if (diaIsControllable(cur))
			return cur;
	}
	
	return dflt;
}

struct UIItem *diaGetPrevControl(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	while (newf != ui) {
		newf--;
		
		if (diaIsControllable(newf))
			return newf;
	}
	
	return cur;
}

/// finds first control on previous line...
struct UIItem *diaGetPrevLine(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	int lb = 0;
	int hadCtrl = 0; // had the scanned line any control?
	
	while (newf != ui) {
		newf--;
		
		if ((lb > 0) && (diaIsControllable(newf)))
			hadCtrl = 1;
		
		if (diaShouldBreakLine(newf)) {// is this a line break?
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

struct UIItem *diaGetNextLine(struct UIItem* cur, struct UIItem *ui) {
	struct UIItem *newf = cur;
	
	int lb = 0;
	
	while (newf->type != UI_TERMINATOR) {
		newf++;
		
		if (diaShouldBreakLine(newf)) {// is this a line break?
				lb++;
		}
		
		if (lb == 1)
			return diaGetNextControl(newf, cur);
	}
	
	return cur;
}

int diaExecuteDialog(struct UIItem *ui) {
	struct UIItem *cur = diaGetFirstControl(ui);
	
	// what? no controllable item? Exit!
	if (cur == ui)
		return -1;
	
	int haveFocus = 0;
	
	// slower controls for dialogs
	setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
	setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);

	// okay, we have the first selectable item
	// we can proceed with rendering etc. etc.
	while (1) {
		diaRenderUI(ui, cur, haveFocus);
		
		readPads();
		
		if (haveFocus) {
			haveFocus = diaHandleInput(cur);
			
			if (!haveFocus) {
				setButtonDelay(KEY_UP, DIA_SCROLL_SPEED);
				setButtonDelay(KEY_DOWN, DIA_SCROLL_SPEED);
			}
		} else {
			if (getKey(KEY_LEFT)) {
				struct UIItem *newf = diaGetPrevControl(cur, ui);
				
				if (newf == cur)
					cur = diaGetLastControl(ui);
				else 
					cur = newf;
			}
			
			if (getKey(KEY_RIGHT)) {
				struct UIItem *newf = diaGetNextControl(cur, cur);
				
				if (newf == cur)
					cur = diaGetFirstControl(ui);
				else 
					cur = newf;
			}
			
			if(getKey(KEY_UP)) {
				// find
				struct UIItem *newf = diaGetPrevLine(cur, ui);
				
				if (newf == cur)
					cur = diaGetLastControl(ui);
				else 
					cur = newf;
			}
			
			if(getKey(KEY_DOWN)) {
				// find
				struct UIItem *newf = diaGetNextLine(cur, ui);
				
				if (newf == cur)
					cur = diaGetFirstControl(ui);
				else 
					cur = newf;
			}
			
			// circle breaks focus or exits with false result
			if (getKeyOn(KEY_CIRCLE)) {
					updateScrollSpeed();
					return 0;
			}
			
			// see what key events we have
			if (getKeyOn(KEY_CROSS)) {
				haveFocus = 1;
				
				if (cur->type == UI_BUTTON) {
					updateScrollSpeed();
					return cur->id;
				}

				if (cur->type == UI_OK) {
					updateScrollSpeed();
					return 1;
				}
			}
		}
	}
}

struct UIItem* diaFindByID(struct UIItem* ui, int id) {
	while (ui->type != UI_TERMINATOR) {
		if (ui->id == id)
			return ui;
			
		ui++;
	}
	
	return NULL;
}

int diaGetInt(struct UIItem* ui, int id, int *value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if ((item->type != UI_INT) && (item->type != UI_BOOL) && (item->type != UI_ENUM))
		return 0;
	
	*value = item->intvalue.current;
	return 1;
}

int diaSetInt(struct UIItem* ui, int id, int value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if ((item->type != UI_INT) && (item->type != UI_BOOL) && (item->type != UI_ENUM))
		return 0;
	
	item->intvalue.def = value;
	item->intvalue.current = value;
	return 1;
}

int diaGetString(struct UIItem* ui, int id, char *value) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_STRING)
		return 0;
	
	strncpy(value, item->stringvalue.text, 32);
	return 1;
}

int diaSetString(struct UIItem* ui, int id, const char *text) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_STRING)
		return 0;
	
	strncpy(item->stringvalue.def, text, 32);
	strncpy(item->stringvalue.text, text, 32);
	return 1;
}

int diaGetColour(struct UIItem* ui, int id, unsigned char *col) {
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

int diaSetColour(struct UIItem* ui, int id, const unsigned char *col) {
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

int diaSetLabel(struct UIItem* ui, int id, const char *text) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_LABEL)
		return 0;
	
	item->label.text = text;
	return 1;
}

int diaSetEnum(struct UIItem* ui, int id, const char **enumvals) {
	struct UIItem *item = diaFindByID(ui, id);
	
	if (!item)
		return 0;
	
	if (item->type != UI_ENUM)
		return 0;
	
	item->intvalue.enumvalues = enumvals;
	return 1;
}
