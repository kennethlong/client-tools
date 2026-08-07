
# YOUR ANGLE (Cursor) — the offline-vs-connected asymmetry, exhaustively

The defect reproduces offline and never with a server connection. That asymmetry is the sharpest
lever we have and I want it enumerated rather than guessed.

Produce an **exhaustive file:line list of every predicate, branch, flag or early return** in the
client's object-creation / world-add / spatial-registration path whose VALUE DIFFERS between:

  (a) a scene entered offline via `Game::setScene(...)` with `Game::setSinglePlayer(true)`, and
  (b) a scene entered with a live server connection.

Include, at minimum, anything reading: single-player state, whether a scene/network exists, whether
the player object is authoritative, whether an object has a valid NetworkId, and anything gated on
objects being server-streamed vs client-created.

For each entry give: file:line, the predicate as written, its value in (a), its value in (b), and
what behaviour changes as a consequence.

I want completeness and precision over narrative. Do not tell me which one you think is responsible
until the end, and label that clearly as opinion separated from the enumeration.
