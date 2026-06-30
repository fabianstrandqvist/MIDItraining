from math import inf

import numpy

# Pitch class 0 = C. Sharps only (flats are a display concern, same ints).
note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# A quality = a name + its interval formula (semitones above the root).
# Four qualities x 12 roots = 48 states. The 6th and the min7 are added on
# purpose: a 6 chord and the min7 a minor-third below share the SAME notes
# (e.g. C6 and Am7 are both {C E G A}), so the notes alone can't tell them
# apart -- only context can.
chords = [
    ("major", [0, 4, 7]),
    ("minor", [0, 3, 7]),
    ("6",     [0, 4, 7, 9]),   # major triad + 6th
    ("min7",  [0, 3, 7, 10]),  # minor triad + flat 7th
]

# Construct all states.
# Each state = one chord rooted somewhere: its name + its pitch-class SET
# (the formula transposed to that root, mod 12). The index is its row/col in
# the transition/emission matrices you'll build next.
states = []
for quality, intervals in chords:
    for root in range(12):
        pcs = frozenset((root + iv) % 12 for iv in intervals)
        name = f"{note_names[root]} {quality}"
        states.append({"name": name, "root": root, "quality": quality, "pcs": pcs})

# Fast lookup from (root, quality) -> state index, so the transition seeding
# below can name its targets musically instead of doing fragile "12 + ..." math.
index_of = {(s["root"], s["quality"]): i for i, s in enumerate(states)}


def states_matching(pcs):
    """Which states exactly equal this observed pitch-class set? Today {0,4,7}
    hits exactly one; once you add 6th/7th qualities, {0,4,7,9} will hit two —
    that two-way tie is exactly what context is there to break."""
    pcs = frozenset(pcs)
    return [i for i, s in enumerate(states) if s["pcs"] == pcs]


# ---------------------------------------------------------------------------
# TODO (yours): Table 1 — transitions A[i][j] = P(next = j | current = i).
#   24x24 numpy array. Hand-seed a few strong transitions per chord
#   (circle of fifths, ii-V-I), spread the rest, normalize each ROW to sum 1.
#
# TODO (yours): Table 2 — emission P(observed notes | state j).
#   Start trivial: ~1 if observed pcs == states[j]["pcs"], tiny eps otherwise.
#
# TODO (yours): the greedy update you derived from Bayes:
#       score[j] = log emission_j(obs) + log A[prev][j]   ->  argmax over j
#   Proof to chase: feed {0,4,7,9} after G7 vs after Em and watch it flip
#   C6 <-> Am7 (needs 6th/7th qualities added so there are two candidates).
# ---------------------------------------------------------------------------

# The three major chords and three minor chords within any quarter of the circle belong to the same key and thus sound good together.


# Emission
def emission(obs, j, eps=1e-6):
    obs = frozenset(obs)
    # TODO: return ~1.0 when obs matches this state's pcs, eps otherwise
    #       (states[j]["pcs"] is the set to compare against)
    return 1.0 if obs == states[j]["pcs"] else eps


def best_state(obs, prev, transitions):
    best_j, best_score = None, -inf
    for j in range(len(states)):
        # score = how well notes fit state j  ×  how likely prev → j
        # TODO: combine emission(obs, j) with transitions[prev][j]
        # if score > best_score: remember j
        score = emission(obs, j) * transitions[prev, j]
        if score > best_score:
            best_j, best_score = j, score

    return best_j


# Transition
# Simplistic hand-seeded transition matrix to understand HMM better.
manual_transitions = numpy.full((len(states), len(states)), 0.01)


def link(i, root, quality, weight):
    """Seed transitions[i] -> (root, quality), wrapping the root mod 12."""
    manual_transitions[i, index_of[(root % 12, quality)]] = weight


for i, s in enumerate(states):
    r, q = s["root"], s["quality"]
    # Staying on the same chord is common.
    manual_transitions[i, i] = 5.0

    # A 6 chord moves like the major triad on its root; a min7 like the minor
    # triad on its root. And wherever a triad is a likely target, its 6/min7
    # colouring is a (weaker) target too -- that's what gives C6 and Am7 any
    # incoming probability at all, so the tie-break has something to work with.
    base = "major" if q in ("major", "6") else "minor"

    if base == "major":
        link(i, r + 9, "minor", 3.0)   # relative minor (vi)
        link(i, r + 9, "min7",  2.0)   #   ...or its min7 colouring
        link(i, r + 7, "major", 3.5)   # perfect fifth (V)
        link(i, r + 7, "6",     2.0)
        link(i, r + 5, "major", 3.5)   # perfect fourth (IV)
        link(i, r + 5, "6",     2.0)
    else:  # minor
        link(i, r + 3, "major", 4.0)   # relative major
        link(i, r + 3, "6",     2.5)
        link(i, r + 5, "minor", 2.5)   # iv minor
        link(i, r + 5, "min7",  1.5)
        link(i, r + 7, "minor", 2.5)   # v minor
        link(i, r + 7, "min7",  1.5)

# Normalize each row to sum to 1
manual_transitions /= manual_transitions.sum(axis=1, keepdims=True)


if __name__ == "__main__":
    print(f"{len(states)} states:")
    for i, s in enumerate(states):
        notes = " ".join(note_names[p] for p in sorted(s["pcs"]))
        print(f"  {i:2d}  {s['name']:<9}  {{{notes}}}")

    print("\nmatching {0,4,7}:  ", states_matching({0, 4, 7}))
    print("matching {0,3,7}:  ", states_matching({0, 3, 7}))

    # The payoff: {C E G A} matches BOTH C6 and Am7, so emission ties them.
    # The previous chord -- via the transition table -- breaks the tie.
    obs = {0, 4, 7, 9}
    ties = states_matching(obs)
    print("matching {0,4,7,9}:", ties, "=", [states[j]["name"] for j in ties])
    for prev_name, prev in (("G major", index_of[(7, "major")]),
                            ("E minor", index_of[(4, "minor")])):
        winner = best_state(obs, prev, manual_transitions)
        print(f"  same notes after {prev_name} -> {states[winner]['name']}")
