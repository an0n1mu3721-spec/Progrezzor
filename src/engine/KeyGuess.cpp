// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#include "KeyGuess.h"

#include <algorithm>
#include <cmath>

namespace engine
{
namespace
{
struct Candidate
{
    int tonic = 0;
    bool minor = false;
};

bool chordFitsKey(const GuessChord& chord, int tonic, bool minor)
{
    const Chord relative { wrap12(chord.root - tonic), chord.type, kAutoInversion };
    if (chordFitsScale(relative, minor ? ScaleId::Minor : ScaleId::Major))
        return true;
    // In a minor key the major chord on the fifth step also fits (E in A Minor)
    return minor && chord.type == ChordType::Major && relative.root == 7;
}

int fitCount(const std::vector<GuessChord>& chords, int tonic, bool minor)
{
    return (int) std::count_if(chords.begin(), chords.end(), [&] (const GuessChord& c) { return chordFitsKey(c, tonic, minor); });
}

int timesRootUsed(const std::vector<GuessChord>& chords, int root)
{
    return (int) std::count_if(chords.begin(), chords.end(), [&] (const GuessChord& c) { return c.root == root; });
}

// The chord of the key: the major chord on a major tonic, the minor chord on a minor tonic
bool isKeyChord(const GuessChord& chord, const Candidate& key)
{
    return chord.root == key.tonic && chord.type == (key.minor ? ChordType::Minor : ChordType::Major);
}
}

KeyGuess guessKeyFromShare(const std::vector<GuessChord>& chords, double minShare)
{
    KeyGuess none;
    if (chords.empty())
        return none;

    const auto& first = chords.front();
    const auto& last = chords.back();
    const int total = (int) chords.size();
    const int need = (int) std::ceil(minShare * total - 1.0e-9);

    // The keys that fit best: the most chords fit them. A major key and its minor twin (3 semitones under) are settled between
    // themselves first.
    int best = 0;
    std::vector<Candidate> candidates;
    for (int major = 0; major < 12; ++major)
    {
        const int minorTonic = wrap12(major + 9);
        const int majorFits = fitCount(chords, major, false);
        const int minorFits = fitCount(chords, minorTonic, true);
        best = std::max({ best, majorFits, minorFits });
    }
    if (best == 0 || best < need)
        return none;

    for (int major = 0; major < 12; ++major)
    {
        const int minorTonic = wrap12(major + 9);
        const bool majorFits = fitCount(chords, major, false) == best;
        const bool minorFits = fitCount(chords, minorTonic, true) == best;
        if (majorFits && minorFits)
        {
            const Candidate asMinor { minorTonic, true }, asMajor { major, false };
            if (isKeyChord(first, asMinor) || isKeyChord(last, asMinor))
                candidates.push_back(asMinor);
            else if (isKeyChord(first, asMajor) || isKeyChord(last, asMajor))
                candidates.push_back(asMajor);
            else
                candidates.push_back(timesRootUsed(chords, minorTonic) > timesRootUsed(chords, major) ? asMinor : asMajor);
        }
        else if (majorFits)
            candidates.push_back({ major, false });
        else if (minorFits)
            candidates.push_back({ minorTonic, true });
    }

    Candidate chosen = candidates.front();
    if (candidates.size() > 1)
    {
        // The key whose chord is the first or the last chord, if that is one key
        std::vector<Candidate> ends;
        for (const auto& c : candidates)
            if (first.root == c.tonic || last.root == c.tonic)
                ends.push_back(c);

        const auto& pool = ends.size() == 1 ? ends : (ends.empty() ? candidates : ends);
        if (pool.size() == 1)
            chosen = pool.front();
        else
        {
            // Else the one whose key chord is used most; a tie gives no guess
            int bestIndex = -1, bestCount = -1;
            bool tie = false;
            for (int i = 0; i < (int) pool.size(); ++i)
            {
                const int count = timesRootUsed(chords, pool[(size_t) i].tonic);
                if (count > bestCount)
                {
                    bestIndex = i;
                    bestCount = count;
                    tie = false;
                }
                else if (count == bestCount)
                    tie = true;
            }
            if (tie)
                return none;
            chosen = pool[(size_t) bestIndex];
        }
    }

    KeyGuess guess;
    guess.found = true;
    guess.key = { chosen.tonic, chosen.minor ? ScaleId::Minor : ScaleId::Major };
    return guess;
}

KeyGuess guessKey(const std::vector<GuessChord>& chords)
{
    return guessKeyFromShare(chords, 1.0);
}

bool sameKeyKind(const Key& a, const Key& b)
{
    return a.tonic == b.tonic && (a.scale == ScaleId::Major) == (b.scale == ScaleId::Major);
}
}
