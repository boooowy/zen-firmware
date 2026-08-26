#!/usr/bin/env python3
"""Generate config/keymap.json for companion apps (zen-hud).

config/keymap.keymap is committed straight to main by the Keymap Editor bot, so
a companion app cannot rely on a hand-maintained copy of the keymap. This script
turns the keymap into a stable, versioned JSON document that an app can just
load: key labels per layer, physical geometry, and the combo definitions with
their timing.

Key labels come from keymap-drawer, which already knows how to preprocess and
parse ZMK devicetree. Everything keymap-drawer drops on the floor but a HUD
needs -- layer node names, combo node names, combo timing -- is read straight
out of the keymap here.

Usage:
    tools/build_keymap_json.py [--keymap PATH] [--info PATH] [--out PATH]
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

SCHEMA_VERSION = 1

# ZMK devicetree binding defaults for zmk,combos.
COMBO_DEFAULT_TIMEOUT_MS = 50
COMBO_DEFAULT_REQUIRE_PRIOR_IDLE_MS = 0

# The matrix transform in boards/shields/zen/zen.dtsi puts the left half on
# columns 0-6 and the right half on columns 7-13. In info.json that shows up as
# a gap in the x coordinate: the left half never goes past 6, the right half
# never starts before 8.
LEFT_HALF_MAX_X = 6

# Behaviours whose first parameter is a layer. keymap-drawer renders these as a
# bare number, which reads as a digit key on the number layers, so the target is
# carried through explicitly and relabelled by the app.
LAYER_BEHAVIOURS = {"mo", "lt", "to", "tog", "sl"}


def run_keymap_drawer(keymap_path: Path) -> dict:
    """Parse the keymap into keymap-drawer's intermediate representation."""
    exe = shutil.which("keymap")
    if exe is None:
        sys.exit("keymap-drawer not found. Install it with: pip install keymap-drawer")

    result = subprocess.run(
        [exe, "parse", "-z", str(keymap_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return yaml.safe_load(result.stdout)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def extract_block(text: str, header_pattern: str) -> str | None:
    """Return the body of the first `<header> { ... }` block, braces balanced."""
    match = re.search(header_pattern, text)
    if match is None:
        return None

    start = text.index("{", match.end() - 1 if text[match.end() - 1] == "{" else match.start())
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1 : i]
    return None


def split_child_nodes(body: str) -> list[tuple[str, str]]:
    """Split a devicetree node body into (child name, child body) pairs."""
    children: list[tuple[str, str]] = []
    pos = 0
    while True:
        match = re.compile(r"([A-Za-z_][\w\-]*)\s*(?::\s*[\w\-]+\s*)?\{").search(body, pos)
        if match is None:
            return children

        start = match.end() - 1
        depth = 0
        for i in range(start, len(body)):
            if body[i] == "{":
                depth += 1
            elif body[i] == "}":
                depth -= 1
                if depth == 0:
                    children.append((match.group(1), body[start + 1 : i]))
                    pos = i + 1
                    break
        else:
            return children


def parse_int_list(child_body: str, prop: str) -> list[int] | None:
    match = re.search(rf"\b{re.escape(prop)}\s*=\s*<([^>]*)>", child_body)
    if match is None:
        return None
    return [int(tok) for tok in re.findall(r"-?\d+", match.group(1))]


def parse_int(child_body: str, prop: str) -> int | None:
    values = parse_int_list(child_body, prop)
    return values[0] if values else None


def parse_bindings(layer_body: str) -> list[str]:
    """Split a layer bindings array into one string per key.

    Every ZMK binding starts with an ampersand and key names spell symbols out
    (AMPERSAND, never a literal &), so splitting on & yields exactly one token
    per key.
    """
    match = re.search(r"\bbindings\s*=\s*<(.*?)>\s*;", layer_body, re.S)
    if match is None:
        return []
    return [
        "&" + re.sub(r"\s+", " ", token.strip())
        for token in match.group(1).split("&")
        if token.strip()
    ]


def binding_metadata(binding: str) -> dict:
    parts = binding.lstrip("&").split(" ")
    action = parts[0]
    meta: dict = {"binding": binding, "action": action}
    if action in LAYER_BEHAVIOURS and len(parts) > 1 and parts[1].isdigit():
        meta["layer"] = int(parts[1])
    return meta


def parse_keymap_source(keymap_path: Path) -> tuple[list[tuple[str, list[str]]], list[dict]]:
    """Read layer nodes and combo definitions out of the keymap itself."""
    text = strip_comments(keymap_path.read_text(encoding="utf-8"))

    layer_sources: list[tuple[str, list[str]]] = []
    keymap_body = extract_block(text, r"keymap\s*\{")
    if keymap_body is not None:
        layer_sources = [
            (name, parse_bindings(body)) for name, body in split_child_nodes(keymap_body)
        ]

    combos: list[dict] = []
    combos_body = extract_block(text, r"combos\s*\{")
    if combos_body is not None:
        for name, body in split_child_nodes(combos_body):
            positions = parse_int_list(body, "key-positions")
            if positions is None:
                continue
            combos.append(
                {
                    "name": name,
                    "positions": positions,
                    "layers": parse_int_list(body, "layers"),
                    "timeout_ms": parse_int(body, "timeout-ms") or COMBO_DEFAULT_TIMEOUT_MS,
                    "require_prior_idle_ms": (
                        parse_int(body, "require-prior-idle-ms")
                        if parse_int(body, "require-prior-idle-ms") is not None
                        else COMBO_DEFAULT_REQUIRE_PRIOR_IDLE_MS
                    ),
                }
            )

    return layer_sources, combos


def normalise_key(entry, binding: str | None) -> dict:
    """Flatten a keymap-drawer key entry into {tap, hold, type} plus binding info."""
    if isinstance(entry, dict):
        key = {
            "tap": str(entry.get("t", "")),
            "hold": str(entry["h"]) if entry.get("h") is not None else None,
            "type": entry.get("type"),
        }
    else:
        key = {"tap": str(entry), "hold": None, "type": None}

    if binding is not None:
        key.update(binding_metadata(binding))
    return key


def build_geometry(info_path: Path) -> list[dict]:
    layouts = json.loads(info_path.read_text(encoding="utf-8"))["layouts"]
    layout = next(iter(layouts.values()))["layout"]

    return [
        {
            "position": position,
            "row": key["row"],
            "col": key["col"],
            "x": key["x"],
            "y": key["y"],
            "side": "left" if key["x"] <= LEFT_HALF_MAX_X else "right",
        }
        for position, key in enumerate(layout)
    ]


def git_commit(repo_root: Path) -> str | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keymap", type=Path, default=repo_root / "config/keymap.keymap")
    parser.add_argument("--info", type=Path, default=repo_root / "config/info.json")
    parser.add_argument("--out", type=Path, default=repo_root / "config/keymap.json")
    args = parser.parse_args()

    parsed = run_keymap_drawer(args.keymap)
    layer_sources, source_combos = parse_keymap_source(args.keymap)
    geometry = build_geometry(args.info)

    layers = []
    for index, (key, entries) in enumerate(parsed.get("layers", {}).items()):
        name, bindings = (
            layer_sources[index] if index < len(layer_sources) else (f"layer_{index}", [])
        )
        layers.append(
            {
                "index": index,
                "id": key,
                "name": name,
                "keys": [
                    normalise_key(entry, bindings[position] if position < len(bindings) else None)
                    for position, entry in enumerate(entries)
                ],
            }
        )

    for layer in layers:
        if len(layer["keys"]) != len(geometry):
            sys.exit(
                f"layer {layer['name']} has {len(layer['keys'])} keys but the layout has "
                f"{len(geometry)}; config/info.json and config/keymap.keymap disagree"
            )

    # keymap-drawer resolves each combo to a display label; the keymap source has
    # the name and timing. Match them on the key-position set.
    labels = {
        tuple(sorted(combo.get("p", []))): combo.get("k")
        for combo in parsed.get("combos", [])
    }
    combos = [
        {**combo, "key": labels.get(tuple(sorted(combo["positions"])))}
        for combo in source_combos
    ]

    document = {
        "schema": SCHEMA_VERSION,
        "source_commit": git_commit(repo_root),
        "keyboard": "ZEN",
        "geometry": geometry,
        "layers": layers,
        "combos": combos,
    }

    args.out.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(
        f"wrote {args.out.relative_to(repo_root)}: "
        f"{len(layers)} layers, {len(geometry)} keys, {len(combos)} combos"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
