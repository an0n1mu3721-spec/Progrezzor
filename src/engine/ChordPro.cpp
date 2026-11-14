// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "ChordPro.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>

#include "QuickText.h"

namespace engine
{
namespace
{
// Minor, Dorian and Phrygian are written as minor. Major, Mixolydian and Lydian are written as major (a ChordPro key has no
// other kinds), so a Mixolydian or Lydian bar is read back as its major key.
bool writtenAsMinor(ScaleId scale)
{
    return scale == ScaleId::Minor || scale == ScaleId::Dorian || scale == ScaleId::Phrygian;
}

std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char) s[a]))
        ++a;
    while (b > a && std::isspace((unsigned char) s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    for (auto& c : s)
        c = (char) std::tolower((unsigned char) c);
    return s;
}

// Where a { or [ is closed, on the same line. npos if it is not.
size_t findClose(const std::string& text, size_t open, char close)
{
    for (size_t i = open + 1; i < text.size(); ++i)
    {
        if (text[i] == close)
            return i;
        if (text[i] == '\n' || text[i] == '\r')
            return std::string::npos;
    }
    return std::string::npos;
}

// The chord words, the notes and the labels that are read from brackets
bool isChordWord(const std::string& content)
{
    if (content.empty() || content[0] < 'A' || content[0] > 'G')
        return false;
    int root = 0;
    ChordType type = ChordType::Major;
    return parseChordName(content, root, type);
}

// [f#] [e] [bb]: a single note in lowercase, which is skipped
bool isNoteWord(const std::string& content)
{
    if (content.empty() || content[0] < 'a' || content[0] > 'g')
        return false;
    return content.size() == 1 || (content.size() == 2 && (content[1] == '#' || content[1] == 'b'));
}

// [VERSE], [Chorus 2], [PRE-CHORUS]: a label of a part of the song, which is skipped
bool isSectionLabel(const std::string& content)
{
    auto word = lower(content);
    while (! word.empty() && (std::isdigit((unsigned char) word.back()) || std::isspace((unsigned char) word.back())))
        word.pop_back();
    static const char* const labels[] = { "intro", "verse", "chorus", "bridge", "outro", "solo", "interlude", "coda", "pre-chorus" };
    return std::any_of(std::begin(labels), std::end(labels), [&] (const char* label) { return word == label; });
}

// How many chords are in brackets in a piece of text (used for the comments and tabs that are skipped)
int countChordBrackets(const std::string& text)
{
    int count = 0;
    size_t i = 0;
    while ((i = text.find('[', i)) != std::string::npos)
    {
        const size_t end = text.find(']', i + 1);
        if (end == std::string::npos)
            break;
        if (isChordWord(trim(text.substr(i + 1, end - i - 1))))
            ++count;
        i = end + 1;
    }
    return count;
}

// Whether a word of the file is "capo", as a word of its own (not inside another word)
bool mentionsCapo(const std::string& text)
{
    const auto low = lower(text);
    for (size_t at = low.find("capo"); at != std::string::npos; at = low.find("capo", at + 1))
    {
        const bool before = at == 0 || ! std::isalpha((unsigned char) low[at - 1]);
        const bool after = at + 4 >= low.size() || ! std::isalpha((unsigned char) low[at + 4]);
        if (before && after)
            return true;
    }
    return false;
}

// Valid UTF-8, as a file of text must be
bool isValidUtf8(const std::string& s)
{
    size_t i = 0;
    while (i < s.size())
    {
        const unsigned char c = (unsigned char) s[i];
        if (c < 0x80)
        {
            ++i;
            continue;
        }
        size_t length = 0;
        unsigned long code = 0;
        unsigned long smallest = 0;
        if ((c & 0xE0) == 0xC0)
        {
            length = 2;
            code = c & 0x1F;
            smallest = 0x80;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            length = 3;
            code = c & 0x0F;
            smallest = 0x800;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            length = 4;
            code = c & 0x07;
            smallest = 0x10000;
        }
        else
            return false;
        if (i + length > s.size())
            return false;
        for (size_t k = 1; k < length; ++k)
        {
            const unsigned char continuation = (unsigned char) s[i + k];
            if ((continuation & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (continuation & 0x3F);
        }
        if (code < smallest || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
            return false;
        i += length;
    }
    return true;
}

// Where the directive with one of two names is, from `from` on: the { and the position after its }. npos if there is none.
struct DirectiveSpan
{
    size_t begin = std::string::npos;
    size_t after = std::string::npos;
};

DirectiveSpan findDirective(const std::string& text, size_t from, const char* first, const char* second)
{
    for (size_t search = from; search < text.size();)
    {
        const size_t brace = text.find('{', search);
        if (brace == std::string::npos)
            break;
        const size_t close = text.find('}', brace);
        if (close == std::string::npos)
            break;
        const auto inner = trim(text.substr(brace + 1, close - brace - 1));
        const auto name = lower(trim(inner.substr(0, inner.find_first_of(": \t"))));
        if (name == first || name == second)
            return { brace, close + 1 };
        search = close + 1;
    }
    return {};
}

// The directives whose text is a comment: the chords in them are not read
bool isCommentDirective(const std::string& name)
{
    return name == "comment" || name == "c" || name == "ci" || name == "comment_italic" || name == "cb" || name == "comment_box";
}

// One bracket of the text, classified: a chord is read, the rest is counted or skipped
void readBracket(const std::string& content, ChordProResult& result)
{
    if (content.empty() || content.find_first_not_of("-|,;:.") == std::string::npos) // notation: removed silently
        return;
    if (isSectionLabel(content))
        return;
    if (isChordWord(content))
    {
        result.chords.push_back(content);
        return;
    }
    if (isNoteWord(content))
        ++result.skippedNotes;
    // any other bracket ([guitar], [N.C.], [*Coda]) is text and is skipped without a count
}

// G, F#m, Bb, Am, Dminor ... Gives false if it is not a key we can read
bool readKey(const std::string& text, ChordProResult& out)
{
    const auto word = trim(text);
    if (word.empty())
        return false;

    int pitch = -1;
    switch (std::toupper((unsigned char) word[0]))
    {
        case 'C': pitch = 0; break;
        case 'D': pitch = 2; break;
        case 'E': pitch = 4; break;
        case 'F': pitch = 5; break;
        case 'G': pitch = 7; break;
        case 'A': pitch = 9; break;
        case 'B': pitch = 11; break;
        default: return false;
    }
    size_t pos = 1;
    if (pos < word.size() && word[pos] == '#')
    {
        ++pitch;
        ++pos;
    }
    else if (pos < word.size() && word[pos] == 'b')
    {
        --pitch;
        ++pos;
    }

    const auto rest = lower(word.substr(pos));
    if (rest == "" || rest == "maj" || rest == "major")
        out.keyMinor = false;
    else if (rest == "m" || rest == "min" || rest == "minor")
        out.keyMinor = true;
    else
        return false;

    out.keyTonic = wrap12(pitch);
    out.keyName = word;
    out.hasKey = true;
    return true;
}
// 0.5, 1.5, 2: no zeros at the end
std::string formatBarsText(double bars)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.2f", bars);
    std::string s = text;
    while (! s.empty() && s.back() == '0')
        s.pop_back();
    if (! s.empty() && s.back() == '.')
        s.pop_back();
    return s;
}

// The cells in a measure from shape="4x4", "1+4x4+1" or "4x4"; 0 if the shape does not say
int cellsFromShape(const std::string& argument)
{
    std::string shape = argument;
    const auto equals = shape.find('=');
    if (equals != std::string::npos)
        shape = shape.substr(equals + 1);
    shape.erase(std::remove_if(shape.begin(), shape.end(), [] (char c) { return c == '"' || c == '\'' || std::isspace((unsigned char) c); }), shape.end());

    // left+measures x beats+right: the part with the x
    size_t start = 0;
    while (start <= shape.size())
    {
        size_t plus = shape.find('+', start);
        const auto part = shape.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        const auto x = part.find_first_of("xX");
        if (x != std::string::npos)
            return std::atoi(part.substr(x + 1).c_str());
        if (plus == std::string::npos)
            break;
        start = plus + 1;
    }
    return 0;
}

struct GridMeasure
{
    std::vector<std::string> cells;
};

// The grid between {start_of_grid ...} and {end_of_grid}: the blocks as Quick-text words
void readGrid(const std::string& body, int shapeCells, std::vector<std::string>& words)
{
    std::vector<GridMeasure> measures; // what was read (for % and %%)
    bool haveBlock = false;
    std::string blockWord;
    double blockBars = 0.0;
    const auto finish = [&]
    {
        if (haveBlock)
            words.push_back(blockWord + (std::abs(blockBars - 1.0) > 0.005 ? ":" + formatBarsText(blockBars) : std::string()));
        haveBlock = false;
    };
    const auto addMeasure = [&] (const GridMeasure& measure)
    {
        const int n = shapeCells > 0 ? shapeCells : (int) measure.cells.size();
        if (n <= 0)
            return;
        for (const auto& cell : measure.cells)
        {
            if (cell == "." || cell == "/")
            {
                if (haveBlock)
                    blockBars += 1.0 / n;
                continue;
            }
            // Several chords in a cell (A~B): the first one
            auto chord = cell.substr(0, cell.find('~'));
            if (chord.empty())
                continue;
            finish();
            std::string upper = chord;
            for (auto& c : upper)
                c = (char) std::toupper((unsigned char) c);
            blockWord = (upper == "N.C." || upper == "NC") ? "R" : chord;
            blockBars = 1.0 / n;
            haveBlock = true;
        }
    };

    int skipNext = 0;
    GridMeasure forcedNext;
    bool haveForced = false;

    size_t lineStart = 0;
    while (lineStart <= body.size())
    {
        size_t lineEnd = body.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = body.size();
        const auto line = body.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        // Tokens: bar symbols (| || |. |: :| :|: |1 :|2>) and cells
        struct Token { bool bar; std::string text; };
        std::vector<Token> tokens;
        size_t i = 0;
        while (i < line.size())
        {
            const char c = line[i];
            if (std::isspace((unsigned char) c))
            {
                ++i;
                continue;
            }
            if (c == '|' || (c == ':' && i + 1 < line.size() && (line[i + 1] == '|' || line[i + 1] == ':')))
            {
                size_t j = i;
                while (j < line.size() && (line[j] == '|' || line[j] == ':' || line[j] == '.'))
                    ++j;
                while (j < line.size() && std::isdigit((unsigned char) line[j]))
                    ++j;
                if (j < line.size() && line[j] == '>')
                    ++j;
                tokens.push_back({ true, line.substr(i, j - i) });
                i = j;
            }
            else
            {
                size_t j = i;
                while (j < line.size() && ! std::isspace((unsigned char) line[j]) && line[j] != '|')
                    ++j;
                tokens.push_back({ false, line.substr(i, j - i) });
                i = j;
            }
        }

        // A line needs a bar symbol. A strum line has S or s right after the first one: not read.
        size_t firstBar = tokens.size();
        for (size_t t = 0; t < tokens.size(); ++t)
            if (tokens[t].bar)
            {
                firstBar = t;
                break;
            }
        if (firstBar == tokens.size())
            continue;
        {
            const auto barEnd = line.find(tokens[firstBar].text) + tokens[firstBar].text.size();
            if (barEnd < line.size() && (line[barEnd] == 's' || line[barEnd] == 'S') && (barEnd + 1 >= line.size() || std::isspace((unsigned char) line[barEnd + 1])))
                continue;
        }

        // The measures: between one bar symbol and the next. What is before the first and after the last is margin.
        size_t t = firstBar;
        while (t < tokens.size())
        {
            size_t next = t + 1;
            GridMeasure measure;
            while (next < tokens.size() && ! tokens[next].bar)
                measure.cells.push_back(tokens[next++].text);
            const bool closed = next < tokens.size(); // a bar symbol ends it
            t = next;
            if (! closed)
                break;
            if (measure.cells.empty())
                continue;

            if (skipNext > 0) // the second measure of a %% is the measure it repeats
            {
                --skipNext;
                if (haveForced)
                    addMeasure(forcedNext);
                haveForced = false;
                measures.push_back(forcedNext);
                continue;
            }
            const auto& first = measure.cells.front();
            if (first == "%" && ! measures.empty())
            {
                const auto again = measures.back();
                addMeasure(again);
                measures.push_back(again);
            }
            else if (first == "%%" && measures.size() >= 2)
            {
                const auto a = measures[measures.size() - 2], b = measures.back();
                addMeasure(a);
                measures.push_back(a);
                forcedNext = b;
                haveForced = true;
                skipNext = 1;
            }
            else if (first == "%" || first == "%%")
                continue;
            else
            {
                addMeasure(measure);
                measures.push_back(measure);
            }
        }
    }
    finish();
}

}

ChordProResult readChordPro(const std::string& text)
{
    ChordProResult result;
    if (text.size() > kMaxChordProBytes)
    {
        result.refusal = "This file is over 256 KB, so it is not imported.";
        return result;
    }
    if (! isValidUtf8(text))
    {
        result.refusal = "This file is not text (UTF-8), so it is not imported.";
        return result;
    }
    result.hasCapo = mentionsCapo(text);

    const size_t length = text.size();
    size_t i = 0;
    bool lineStart = true;
    while (i < length)
    {
        const char c = text[i];
        if (c == '\n' || c == '\r')
        {
            lineStart = true;
            ++i;
            continue;
        }

        if (lineStart)
        {
            lineStart = false;
            size_t first = i;
            while (first < length && (text[first] == ' ' || text[first] == '\t'))
                ++first;
            if (first < length && text[first] == '#') // a comment line: not read, but its chords are counted
            {
                size_t end = i;
                while (end < length && text[end] != '\n' && text[end] != '\r')
                    ++end;
                result.skippedInComments += countChordBrackets(text.substr(i, end - i));
                i = end;
                continue;
            }
        }

        if (c == '{')
        {
            const size_t end = findClose(text, i, '}');
            if (end == std::string::npos) // not closed on its line: just a character
            {
                ++i;
                continue;
            }
            const auto body = trim(text.substr(i + 1, end - i - 1));
            const size_t colon = body.find(':');
            size_t nameEnd = colon;
            if (nameEnd == std::string::npos)
                nameEnd = body.find_first_of(" \t");
            const auto name = lower(trim(body.substr(0, nameEnd == std::string::npos ? body.size() : nameEnd)));
            const auto arg = nameEnd == std::string::npos ? std::string() : trim(body.substr(nameEnd + 1));

            if (name == "title" || name == "t")
                result.title = arg;
            else if (name == "key")
                readKey(arg, result);
            else if (name == "start_of_grid" || name == "sog")
            {
                // Everything up to the end of the grid is the grid
                const auto close = findDirective(text, end + 1, "end_of_grid", "eog");
                const size_t gridEnd = close.begin == std::string::npos ? length : close.begin;
                std::vector<std::string> words;
                readGrid(text.substr(end + 1, gridEnd - (end + 1)), cellsFromShape(arg), words);
                result.chords.insert(result.chords.end(), words.begin(), words.end());
                i = close.begin == std::string::npos ? length : close.after;
                continue;
            }
            else if (isCommentDirective(name))
            {
                ++result.skippedDirectives;
                result.skippedInComments += countChordBrackets(body);
            }
            else if (name == "start_of_comment" || name == "soc" || name == "start_of_tab" || name == "sot")
            {
                // Everything up to the end directive is skipped
                ++result.skippedDirectives;
                const bool tab = name == "start_of_tab" || name == "sot";
                const auto close = tab ? findDirective(text, end + 1, "end_of_tab", "eot") : findDirective(text, end + 1, "end_of_comment", "eoc");
                const size_t blockEnd = close.begin == std::string::npos ? length : close.begin;
                result.skippedInComments += countChordBrackets(text.substr(end + 1, blockEnd - (end + 1)));
                if (close.begin != std::string::npos)
                    ++result.skippedDirectives;
                i = close.begin == std::string::npos ? length : close.after;
                continue;
            }
            else
                ++result.skippedDirectives;
            i = end + 1;
            continue;
        }

        if (c == '[')
        {
            const size_t end = findClose(text, i, ']');
            if (end == std::string::npos) // not closed on its line: just a character
            {
                ++i;
                continue;
            }
            readBracket(trim(text.substr(i + 1, end - i - 1)), result);
            i = end + 1;
            continue;
        }

        ++i;
    }

    if (result.chords.empty())
        result.refusal = "No chords found in that file.";
    else if (result.chords.size() > (size_t) kMaxBlocks)
        result.refusal = "This file has " + std::to_string(result.chords.size()) + " chords. The bar holds " + std::to_string(kMaxBlocks)
                         + ", so the file is not imported.";
    return result;
}

std::string chordProSummary(const ChordProResult& result)
{
    std::string text = "Read " + std::to_string(result.chords.size()) + " chords. Skipped: " + std::to_string(result.skippedDirectives)
                       + " directives, " + std::to_string(result.skippedNotes) + " notes, " + std::to_string(result.skippedInComments)
                       + " chords in comments or tabs, and some text.";
    if (result.hasCapo)
        text += " The file mentions a capo, so the chords are as written.";
    return text;
}

KeyGuess chordProKeyGuess(const ChordProResult& result)
{
    if (result.hasKey || ! result.refusal.empty())
        return {};
    std::vector<GuessChord> chords;
    for (const auto& word : result.chords)
    {
        int root = 0;
        ChordType type = ChordType::Major;
        if (parseChordName(word.substr(0, word.find(':')), root, type)) // a grid word may have its length after a colon
            chords.push_back({ root, type });
    }
    return guessKeyFromShare(chords, 0.75);
}

std::string chordsAsQuickText(const std::vector<std::string>& chords, int maxChords, bool* tooMany)
{
    if (tooMany != nullptr)
        *tooMany = (int) chords.size() > maxChords;
    std::string text;
    for (size_t i = 0; i < chords.size() && (int) i < maxChords; ++i)
    {
        if (i > 0)
            text += ' ';
        text += chords[i];
    }
    return text;
}

namespace
{
// One word of Quick-text ("Am", "C:0.5", "C:2") split into the chord and the length in bars (1 when none is written)
void splitLength(const std::string& word, std::string& chord, std::string& length, double& bars)
{
    const auto colon = word.find(':');
    chord = word.substr(0, colon);
    length = colon == std::string::npos ? std::string() : word.substr(colon + 1);
    bars = length.empty() ? 1.0 : std::atof(length.c_str());
}
}

std::string chordsAsQuickTextBars(const std::vector<std::string>& chords, int maxChords, bool* tooMany)
{
    if (tooMany != nullptr)
        *tooMany = (int) chords.size() > maxChords;
    const size_t count = std::min(chords.size(), (size_t) std::max(0, maxChords));

    std::string text;
    double position = 0.0; // in bars
    const auto onBarLine = [](double bars) { return std::fabs(bars - std::round(bars)) < 1.0e-6; };
    for (size_t i = 0; i < count; ++i)
    {
        std::string chord, length;
        double bars = 1.0;
        splitLength(chords[i], chord, length, bars);
        std::string cell;
        double used = bars;
        if (onBarLine(position) && std::fabs(bars - 1.0) < 1.0e-6)
            cell = chord; // a whole bar
        else if (onBarLine(position) && std::fabs(bars - 0.5) < 1.0e-6 && i + 1 < count)
        {
            std::string nextChord, nextLength;
            double nextBars = 1.0;
            splitLength(chords[i + 1], nextChord, nextLength, nextBars);
            if (std::fabs(nextBars - 0.5) < 1.0e-6)
            {
                cell = chord + " " + nextChord; // two half bars share the bar
                used = 1.0;
                ++i;
            }
            else
                cell = chord + ":" + length;
        }
        else
            cell = length.empty() ? chord + ":1" : chord + ":" + length;
        text += text.empty() ? "| " : " | ";
        text += cell;
        position += used;
    }
    if (!text.empty())
        text += " |";
    return text;
}

std::string exportChordPro(const Progression& progression, const Key& key, double barBeats, const std::string& title)
{
    // How many cells a measure has: its beats, but an even number, so that half a bar is whole cells
    int cells = (int) std::lround(barBeats);
    if (cells < 2)
        cells = 2;
    if (cells % 2 != 0)
        cells *= 2;
    const int measuresPerLine = 4;

    std::string text = "{title: " + title + "}\n";
    text += "{key: " + keyName(key) + (writtenAsMinor(key.scale) ? "m" : "") + "}\n";
    text += "{start_of_grid shape=\"" + std::to_string(measuresPerLine) + "x" + std::to_string(cells) + "\"}\n";

    // The cells in order: a chord, then a dot for each cell it goes on
    std::vector<std::string> stream;
    for (const auto& block : progression)
    {
        const int n = std::max(1, (int) std::lround(block.beats / barBeats * cells));
        stream.push_back(block.rest ? "N.C." : chordName(block.chord, key));
        for (int c = 1; c < n; ++c)
            stream.push_back(".");
    }

    size_t at = 0;
    while (at < stream.size())
    {
        std::string line = "|";
        for (int measure = 0; measure < measuresPerLine && at < stream.size(); ++measure)
        {
            for (int c = 0; c < cells && at < stream.size(); ++c)
                line += " " + stream[at++];
            line += " |";
        }
        text += line + "\n";
    }
    text += "{end_of_grid}\n";
    return text;
}

bool chordProKeyDiffers(const ChordProResult& result, const Key& key)
{
    if (! result.hasKey)
        return false;
    const bool minorNow = writtenAsMinor(key.scale);
    return result.keyTonic != key.tonic || result.keyMinor != minorNow;
}
}
