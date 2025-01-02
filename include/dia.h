#ifndef __DIA_H
#define __DIA_H

// UI dialog item definition
typedef enum {
    // terminates the definition of dialog. Mandatory
    UI_TERMINATOR,
    // Break to next line
    UI_BREAK,
    // Break to next line, draw line splitter
    UI_SPLITTER,
    // A spacer of a few pixels
    UI_SPACER,
    // A string label
    UI_LABEL,
    // Ok button
    UI_OK, // Just a shortcut for BUTTON with OK label and id 1!
    // Universal button (display's label, returns id on X)
    UI_BUTTON,
    // draw integer input box
    UI_INT,
    // draw string input box
    UI_STRING,
    // draw password string input box
    UI_PASSWORD,
    // bool value (On/Off). (Uses int value for storage)
    UI_BOOL,
    // enumeration (multiple strings)
    UI_ENUM,
    // Colour selection
    UI_COLOUR
} UIItemType;

// UI item definition (for dialogues)
struct UIItem
{
    UIItemType type;
    int id; // id of this item
    unsigned char enabled;
    unsigned char visible;
    short int hintId;     // shown if not NULL
    short int fixedWidth; // 0: no fixed width, >0: width in pixels, <0: width in %
    short int fixedHeight;

    union
    {
        struct
        {
            const char *text;
            int stringId; // if negative, the text is used
        } label;

        struct
        { // integer value
            // default value
            int def;
            int current;
            int min;
            int max;
            // if UI_ENUM is used, this contains enumeration values
            const char **enumvalues; // last one has to be NULL
        } intvalue;

        struct
        { // fixed 32 character string (including NULL terminator)
            // default value
            char def[32];
            char text[32];
            int (*handler)(char *text, int maxLen);
        } stringvalue;

        struct
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } colourvalue;
    };
};

/// Dialog display
int diaExecuteDialog(struct UIItem *ui, int uiId, short inMenu, int (*updater)(int modified));
void diaRenderUI(struct UIItem *ui, short inMenu, struct UIItem *cur, int haveFocus);
int diaShowKeyb(char *text, int maxLen, int hide_text, const char *title);
void diaSetEnabled(struct UIItem *ui, int id, int enabled);
void diaSetVisible(struct UIItem *ui, int id, int visible);
void diaSetItemType(struct UIItem *ui, int id, UIItemType type);
int diaGetInt(struct UIItem *ui, int id, int *value);
int diaSetInt(struct UIItem *ui, int id, int value);
int diaGetString(struct UIItem *ui, int id, char *value, int length);
int diaSetString(struct UIItem *ui, int id, const char *text);
int diaGetColor(struct UIItem *ui, int id, unsigned char *col);
int diaSetColor(struct UIItem *ui, int id, const unsigned char *col);
int diaSetU64Color(struct UIItem *ui, int id, u64 col);
// set label pointer into the label's text (must be valid while rendering dialog)
int diaSetLabel(struct UIItem *ui, int id, const char *text);
// sets the current enum value list for given control
int diaSetEnum(struct UIItem *ui, int id, const char **enumvals);

#endif
