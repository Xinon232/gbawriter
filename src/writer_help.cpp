#include "writer_help.h"
namespace writer {
namespace {
// Controls: plain words, the keys you need most first.
constexpr HelpPage files[] = {
    {"Home", {"#Home", "New File: a new diary file.", "A: open a file.", "Hold A + Up/Down: move a file.",
              "Start: Import TXT files.", "Select: menu (Rename, Delete,", "File names, Controls, Credits).",
              "B: back to the open file."}},
    {"New File", {"#Where files go", "TXT files in /gbawriter", "at the root of the SD card.", "#New File",
                  "Up/Down: day, month or year.", "Left/Right: change it.", "A: create.  B: back.",
                  "Select > File names: format."}},
    {"Import", {"#Start on Home", "Browse the whole SD card.", "A: open a folder or import", "a TXT file.  B: up.",
                "#Name already used", "It becomes a numbered copy,", "for example Diary (2).txt.",
                "Nothing is replaced."}},
    {"Rename and Delete", {"#Select on a file", "Rename: type the new name,", "Start+A: save  Start+B: cancel.",
                           "Delete: A, then A on Sure?", "(turn it on in Secret Settings).", "#The open file",
                           "cannot be renamed or deleted:", "open another file first."}}};
constexpr HelpPage letters[] = {
    {"Letters", {"Hold a direction, then", "B / A / R: letter 1 / 2 / 3.", "#Letters", "Up: abc    Right: hij",
                 "Down: nop  Left: tuw", "#Two more", "Hold Right, press R twice: g.", "Hold Left, press R twice: v."}}};
constexpr HelpPage layer[] = {
    {"L layer", {"Hold L and a direction,", "then B / A / R:", "#Letters", "Up: def    Right: klm",
                 "Down: qrs  Left: xyz", "", "L is held, it is never", "switched on and off."}}};
constexpr HelpPage spaces[] = {
    {"Spaces and case", {"#Spaces", "A alone: space.  B: delete.", "Hold A or B alone: repeat.",
                         "Start alone: new line.", "#Case", "Short R alone: Shift.",
                         "Hold R alone (0.8 s): Caps.", "R alone again: back to normal."}}};
constexpr HelpPage symbols[] = {
    {"Symbols", {"Select alone: types a period.", "Keep Select held to change it.", "#While Select is held",
                 "Up/Down: 1 2 3 4 5 6 7 8 9 0", "R: . , ' \" : ! ?   L: back", "Right: ( ) / ; @ # % & _ + = -",
                 "Left: the same, backwards.", "Release Select: keep it."}}};
constexpr HelpPage accents[] = {
    {"Accents", {"#Two ways", "Hold Select, then type a letter:", "it gets its first accent.",
                 "Or type a letter, keep its keys", "held and press Select.", "#More accents",
                 "Keep Select held and press", "B / A / R again."}},
    {"Accent letters", {"a: á ä à â ã å æ", "c: ç č ć     e: é è ë ê", "i: í ï ì î     n: ñ ń",
                        "o: ó ö ô ò õ ø œ", "s: ß š ś     u: ü ú ù û", "y: ý ÿ     z: ž ź ż", "",
                        "Shift and Caps work here too."}}};
constexpr HelpPage caret[] = {
    {"Caret and saving", {"#Hold Start and press", "Left/Right: caret.", "Up/Down: line.  L/R: page.",
                          "Select: status bar on / off.", "#Saving", "Start+A: save.",
                          "Start+B: Home without saving.", "There is no autosave."}},
    {"Back to Home", {"Start+B keeps your text open:", "on Home the file has a play", "mark; B takes you back.",
                      "#Another file", "Opening another file asks:", "Save, Discard or Cancel.", "#Switching off",
                      "loses text that is not saved."}},
    {"Status bar", {"The bar at the bottom shows", "the file name, the letter group", "and Shift / Caps.", "#Start+Select",
                    "hides or shows it while writing.", "#Select > Status bar: On / Off", "how files open (saved).",
                    "Default: On."}}};
// Credits: the personal page first, then the license and each part made by
// others with its license (gbavocab / gbamp3 order).
constexpr HelpPage credits[] = {
    {"Credits", {"gbawriter V4.1", "", "Made by Halim Jarrar", "(C) 2026", "", "halimj.itch.io",
                 "gba@halim-jarrar.de", ""}},
    {"License", {"gbawriter is free software under", "the GNU GPL 3.0 or later.", "", "#Source code",
                 "github.com/Xinon232/gbawriter", "", "Parts made by others and their",
                 "licenses are on the next pages."}},
    {"Text and fonts", {"#Text renderer", "SuperFW by David Guillen Fandos", "License: GPL 3.0 or later", "#Fonts",
                        "UNSCII by Viznut: GPL", "GNU Unifont (also Hangul):", "GPL 2.0 or later",
                        "UI font: from gbamp3 (MIT)"}},
    {"SD card and files", {"#SD card driver", "From SuperFW", "by David Guillen Fandos",
                           "License: GPL 3.0 or later", "#File system", "FatFs by ChaN",
                           "License: FatFs (BSD style)", ""}},
    {"Engine", {"#Engine", "Butano by Gustavo Valiente", "License: zlib", "", "#Built with",
                "devkitPro / devkitARM", "", ""}},
    {"Secret Settings", {"Press A to open", "Secret Settings.", "", "", "", "", "", ""}}};
struct Topic {
  const char *name;
  const HelpPage *pages;
  int count;
};
template <int N> constexpr Topic topic(const char *name, const HelpPage (&p)[N]) { return {name, p, N}; }
constexpr Topic topics[HELP_TOPICS] = {topic("Files and menus", files), topic("Letters", letters),
                                       topic("L layer", layer),         topic("Spaces and case", spaces),
                                       topic("Symbols", symbols),       topic("Accents", accents),
                                       topic("Caret and saving", caret), topic("Credits", credits)};
} // namespace
const char *help_topic_name(int t) { return topics[t].name; }
int help_topic_pages(int t) { return topics[t].count; }
const HelpPage &help_page(int t, int p) { return topics[t].pages[p]; }
bool help_secret_page(int t, int p) { return t == CREDITS_TOPIC && p == topics[t].count - 1; }
} // namespace writer
