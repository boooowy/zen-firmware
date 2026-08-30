#!/usr/bin/env python3
"""Generate zen-hud/keymap.json for companion apps (zen-hud).

config/keymap.keymap is committed straight to main by the Keymap Editor bot, so
a companion app cannot rely on a hand-maintained copy of the keymap. This script
turns the keymap into a stable, versioned JSON document that an app can just
load: key labels per layer, physical geometry, and the combo definitions with
their timing.

The output deliberately does NOT live in config/. That directory belongs to ZMK
and to the Keymap Editor: the editor reads config/*.json as layout metadata and
writes config/keymap.json itself on every save. A file of ours at that path made
the editor refuse to open the repo ("info must define \"layouts\"").

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


# HID keyboard usage page, and the ZMK key names that map onto it.
#
# Needed because a combo can only be recognised on the host by the keycode it
# emits: ZMK consumes the key presses that triggered it, so the presses never
# reach a telemetry listener at all. Matching that keycode back to a combo needs
# the combo's output expressed the way the wire format expresses it.
HID_USAGE_PAGE_KEYBOARD = 0x07

# Modifier bits as HID reports them, in the order ZMK's wrapper functions apply.
MODIFIER_BITS = {
    "LC": 0x01, "LS": 0x02, "LA": 0x04, "LG": 0x08,
    "RC": 0x10, "RS": 0x20, "RA": 0x40, "RG": 0x80,
}

def _keyboard_usages() -> dict[str, int]:
    usages: dict[str, int] = {}

    for index, letter in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
        usages[letter] = 0x04 + index

    digits = ["N1", "N2", "N3", "N4", "N5", "N6", "N7", "N8", "N9", "N0"]
    words = ["NUMBER_1", "NUMBER_2", "NUMBER_3", "NUMBER_4", "NUMBER_5",
             "NUMBER_6", "NUMBER_7", "NUMBER_8", "NUMBER_9", "NUMBER_0"]
    for index, (short, long) in enumerate(zip(digits, words)):
        usages[short] = usages[long] = 0x1E + index

    for index in range(1, 13):
        usages[f"F{index}"] = 0x39 + index

    usages.update({
        "ENTER": 0x28, "RET": 0x28, "RETURN": 0x28,
        "ESC": 0x29, "ESCAPE": 0x29,
        "BSPC": 0x2A, "BACKSPACE": 0x2A,
        "TAB": 0x2B,
        "SPACE": 0x2C, "SPC": 0x2C,
        "MINUS": 0x2D, "UNDER": 0x2D, "UNDERSCORE": 0x2D,
        "EQUAL": 0x2E, "PLUS": 0x2E,
        "LBKT": 0x2F, "LEFT_BRACKET": 0x2F, "LBRC": 0x2F, "LEFT_BRACE": 0x2F,
        "RBKT": 0x30, "RIGHT_BRACKET": 0x30, "RBRC": 0x30, "RIGHT_BRACE": 0x30,
        "BSLH": 0x31, "BACKSLASH": 0x31, "PIPE": 0x31,
        "NON_US_HASH": 0x32, "NON_US_BACKSLASH": 0x64,
        "SEMI": 0x33, "SEMICOLON": 0x33, "COLON": 0x33,
        "APOS": 0x34, "SINGLE_QUOTE": 0x34, "SQT": 0x34, "DQT": 0x34, "DOUBLE_QUOTES": 0x34,
        "GRAVE": 0x35, "TILDE": 0x35,
        "COMMA": 0x36, "LT": 0x36, "LESS_THAN": 0x36,
        "DOT": 0x37, "PERIOD": 0x37, "GT": 0x37, "GREATER_THAN": 0x37,
        "FSLH": 0x38, "SLASH": 0x38, "QMARK": 0x38, "QUESTION": 0x38,
        "CAPS": 0x39, "CAPSLOCK": 0x39,
        "INS": 0x49, "INSERT": 0x49,
        "HOME": 0x4A, "PG_UP": 0x4B, "PAGE_UP": 0x4B,
        "DEL": 0x4C, "DELETE": 0x4C,
        "END": 0x4D, "PG_DN": 0x4E, "PAGE_DOWN": 0x4E,
        "RIGHT": 0x4F, "RIGHT_ARROW": 0x4F,
        "LEFT": 0x50, "LEFT_ARROW": 0x50,
        "DOWN": 0x51, "DOWN_ARROW": 0x51,
        "UP": 0x52, "UP_ARROW": 0x52,
        "LCTRL": 0xE0, "LEFT_CONTROL": 0xE0,
        "LSHFT": 0xE1, "LEFT_SHIFT": 0xE1,
        "LALT": 0xE2, "LEFT_ALT": 0xE2,
        "LGUI": 0xE3, "LEFT_GUI": 0xE3, "LEFT_COMMAND": 0xE3, "LCMD": 0xE3,
        "RCTRL": 0xE4, "RIGHT_CONTROL": 0xE4,
        "RSHFT": 0xE5, "RIGHT_SHIFT": 0xE5,
        "RALT": 0xE6, "RIGHT_ALT": 0xE6,
        "RGUI": 0xE7, "RIGHT_GUI": 0xE7, "RIGHT_COMMAND": 0xE7, "RCMD": 0xE7,
    })

    # Shifted symbols ZMK spells as their own names.
    for name, (base, _) in {
        "EXCLAMATION": ("N1", 0), "EXCL": ("N1", 0),
        "AT_SIGN": ("N2", 0), "AT": ("N2", 0),
        "HASH": ("N3", 0), "POUND": ("N3", 0),
        "DOLLAR": ("N4", 0), "DLLR": ("N4", 0),
        "PERCENT": ("N5", 0), "PRCNT": ("N5", 0),
        "CARET": ("N6", 0),
        "AMPERSAND": ("N7", 0), "AMPS": ("N7", 0),
        "ASTERISK": ("N8", 0), "ASTRK": ("N8", 0), "STAR": ("N8", 0),
        "LEFT_PARENTHESIS": ("N9", 0), "LPAR": ("N9", 0),
        "RIGHT_PARENTHESIS": ("N0", 0), "RPAR": ("N0", 0),
    }.items():
        usages[name] = usages[base]

    return usages

KEYBOARD_USAGES = _keyboard_usages()

# Names ZMK defines as an implicitly shifted key. The HID report carries the
# unshifted usage plus a shift modifier, which is what arrives over telemetry.
IMPLICITLY_SHIFTED = {
    "EXCLAMATION", "EXCL", "AT_SIGN", "AT", "HASH", "POUND", "DOLLAR", "DLLR",
    "PERCENT", "PRCNT", "CARET", "AMPERSAND", "AMPS", "ASTERISK", "ASTRK",
    "STAR", "LEFT_PARENTHESIS", "LPAR", "RIGHT_PARENTHESIS", "RPAR",
    "UNDER", "UNDERSCORE", "PLUS", "PIPE", "TILDE", "COLON", "DQT",
    "DOUBLE_QUOTES", "LBRC", "LEFT_BRACE", "RBRC", "RIGHT_BRACE",
    "QMARK", "QUESTION", "LT", "LESS_THAN", "GT", "GREATER_THAN",
}


def resolve_output(binding: str) -> dict | None:
    """The HID usage a binding emits, or None when it emits no keycode.

    Only &kp is resolved. &bt, &mo and the rest emit nothing a host can see,
    which is why a combo bound to one of them cannot be reported at all.
    """
    parts = binding.lstrip("&").split()
    if len(parts) != 2 or parts[0] != "kp":
        return None

    key = parts[1]
    modifiers = 0

    # Unwrap LC(...) / LS(LA(...)) and friends, accumulating the modifiers.
    while True:
        match = re.fullmatch(r"([LR][CSAG])\((.+)\)", key)
        if match is None:
            break
        modifiers |= MODIFIER_BITS[match.group(1)]
        key = match.group(2)

    usage = KEYBOARD_USAGES.get(key)
    if usage is None:
        return None

    if key in IMPLICITLY_SHIFTED:
        modifiers |= MODIFIER_BITS["LS"]

    return {
        "usage_page": HID_USAGE_PAGE_KEYBOARD,
        "keycode": usage,
        "implicit_mods": modifiers,
    }


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


def parse_string(child_body: str, prop: str) -> str | None:
    match = re.search(rf'\b{re.escape(prop)}\s*=\s*"([^"]*)"', child_body)
    return match.group(1) if match else None


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


def parse_keymap_source(keymap_path: Path) -> tuple[list[dict], list[dict]]:
    """Read layer nodes and combo definitions out of the keymap itself."""
    text = strip_comments(keymap_path.read_text(encoding="utf-8"))

    layer_sources: list[dict] = []
    keymap_body = extract_block(text, r"keymap\s*\{")
    if keymap_body is not None:
        layer_sources = [
            {
                "name": name,
                # ZMK's optional human-readable layer name. Nothing sets it today,
                # but if it ever does the app should prefer it over its own
                # hard-coded role table -- which has already gone stale once.
                "display_name": parse_string(body, "display-name"),
                "bindings": parse_bindings(body),
            }
            for name, body in split_child_nodes(keymap_body)
        ]

    combos: list[dict] = []
    combos_body = extract_block(text, r"combos\s*\{")
    if combos_body is not None:
        for name, body in split_child_nodes(combos_body):
            positions = parse_int_list(body, "key-positions")
            if positions is None:
                continue
            binding_match = re.search(r"\bbindings\s*=\s*<([^>]*)>", body)
            binding = (
                "&" + re.sub(r"\s+", " ", binding_match.group(1).strip().lstrip("&"))
                if binding_match
                else None
            )
            combos.append(
                {
                    "name": name,
                    "binding": binding,
                    "output": resolve_output(binding) if binding else None,
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
    parser.add_argument("--out", type=Path, default=repo_root / "zen-hud/keymap.json")
    args = parser.parse_args()

    parsed = run_keymap_drawer(args.keymap)
    layer_sources, source_combos = parse_keymap_source(args.keymap)
    geometry = build_geometry(args.info)

    layers = []
    for index, (key, entries) in enumerate(parsed.get("layers", {}).items()):
        source = (
            layer_sources[index]
            if index < len(layer_sources)
            else {"name": f"layer_{index}", "display_name": None, "bindings": []}
        )
        bindings = source["bindings"]
        layers.append(
            {
                "index": index,
                "id": key,
                "name": source["name"],
                "display_name": source["display_name"],
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

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(
        f"wrote {args.out.relative_to(repo_root)}: "
        f"{len(layers)} layers, {len(geometry)} keys, {len(combos)} combos"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
