// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "KeyGuess.h"
#include "Progression.h"

// Reading a ChordPro file for its chords. Pure C++, no JUCE. The whole song is read, in the order it is written, and each chord
// becomes one bar. The chords are handed on as plain words, so that Quick-text can read them (and mark what it does not know).
// Nothing is corrected.
//
// What is read:
//   [C] [Am7] [G/B]          a bracket whose content starts with A to G and is a chord name we know (aliases too)
//   {title} {t}, {key}       the title and the key (the key is used only when it can be read)
//   grids                    {start_of_grid} ... {end_of_grid} (short {sog} {eog}): the cells between the bar lines are beats, a
//                            chord starts a block, "." or "/" lets it go on, "N.C." is a pause, % repeats the measure before and
//                            %% the two before. The length of a block is how many cells it takes of a measure (shape="4x4" says
//                            how many cells a measure has, else the cells of the measure itself are counted). Bar line symbols
//                            (|| |: :| |1 ...) only separate measures; repeats are not played out. Margins and strum lines are
//                            skipped. Each block comes out as a Quick-text word with its length in bars (C, C:2, G:0.5).
//
// What is skipped: text outside brackets (lyrics, tips, URLs), # comment lines, every other directive, the lowercase single notes
// ([e] [f#]), notation brackets ([-] [|] [,] [;] [:] [.]), section labels ([VERSE], [CHORUS 2] ...) and every other bracket that is
// not a chord ([guitar], [N.C.], [*Coda]). Chords inside {comment}-type directives, {start_of_comment} blocks and {start_of_tab}
// blocks are skipped too, and counted.
namespace engine
{
// A file larger than this is not read (256 KB)
constexpr size_t kMaxChordProBytes = 256 * 1024;

struct ChordProResult
{
    // Not empty: the file is not imported, and this says why ("No chords found in that file.", too many chords, not text)
    std::string refusal;
    std::vector<std::string> chords; // every chord of the file, in order, one bar each (grid words keep their length)
    std::string title;
    bool hasKey = false;             // the file says {key: ...} and it is one we can read
    std::string keyName;             // as written: G, F#m, Bb
    int keyTonic = 0;                // 0 = C ... 11 = B
    bool keyMinor = false;
    bool hasCapo = false;            // the file mentions a capo, which is not read: the chords are as written
    int skippedDirectives = 0;       // every directive that is not title or key
    int skippedNotes = 0;            // single notes in lowercase: [e] [f#]
    int skippedInComments = 0;       // chords inside comments and tabs
};

ChordProResult readChordPro(const std::string& text);

// The line for the notice after a file is read: "Read 8 chords. Skipped: 2 directives, 1 notes, 0 chords in comments or tabs, and
// some text." (and the capo sentence when the file mentions one)
std::string chordProSummary(const ChordProResult&);

// The key the chords of a file fit, when the file does not say its key: the key that the most chords fit, if at least three in
// four do. Nothing when the file has a key, or is refused.
KeyGuess chordProKeyGuess(const ChordProResult&);

// The chords as Quick-text, one bar each: "C Am F G". At most `maxChords`; `tooMany` says whether some were left out.
std::string chordsAsQuickText(const std::vector<std::string>& chords, int maxChords, bool* tooMany = nullptr);

// The same chords written as a chart with bar lines: | C | Am | F G | C:2 |. A word of one bar is a bar, two half bars that start a bar
// share it, and any other length is written after the chord (C:2, G:0.5). Reads back as exactly the same blocks.
std::string chordsAsQuickTextBars(const std::vector<std::string>& chords, int maxChords, bool* tooMany = nullptr);

// A ChordPro file for the progression: the title, the key and the chords as a grid with the cells of a measure as beats, so that
// the lengths are kept (a chord that goes on is written as ".", a pause as "N.C."). Octaves are not written.
// `barBeats` is the length of a bar in beats.
std::string exportChordPro(const Progression&, const Key&, double barBeats, const std::string& title = "Progrezzor");

// Does the song's key differ from the key of the plug-in? (Same tonic, and both major or both minor, is the same.)
bool chordProKeyDiffers(const ChordProResult&, const Key&);
}
