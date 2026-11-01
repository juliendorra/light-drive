# Light Drive artwork

The indexed PNGs in `png/` are the original game sprites and neon textures.
Palette index 0 is opaque black; index 15 is exterior transparency. `stencils.py`
and `trees.py` retain the editable pixel drawings, with anchors and source
hashes in the JSON manifests. The generated headers at the sketch root are
ready to compile; normal game builds need no art editor.
