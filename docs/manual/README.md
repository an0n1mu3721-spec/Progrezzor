# Manual

- `manual.md` is the manual (plain Markdown, English). It is the only place the text lives.
- `img/` holds the screenshots with arrows. They are made, not drawn by hand:
  `cmake --build build --target ProgrezzorUiTest` and then `python3 tools/make_manual_images.py` (from the project root).
  To move or add an arrow, change the lists in `tools/make_manual_images.py` and run it again.
- Run it from a clean checkout before a release, so the version under the logo has no "-dirty".
- `FOR-PLUGIN-SITE.md` is a note for the Claude Code session in `../plugin-site` (music.henrikjuhl.dk).
