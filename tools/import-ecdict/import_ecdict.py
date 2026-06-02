#!/usr/bin/env python3
import argparse
import csv
import re
import sqlite3
from pathlib import Path

POS_MARKERS = (
    "n",
    "v",
    "a",
    "s",
    "r",
    "adj",
    "adv",
    "vi",
    "vt",
    "prep",
    "pron",
    "conj",
    "interj",
    "int",
    "num",
    "aux",
    "abbr",
    "art",
    "pl",
    "sing",
    "suf",
    "suff",
    "pref",
    "prefix",
    "comb",
    "phrase",
    "phr",
    "idiom",
    "p",
    "imp",
    "un",
    "na",
    "alt",
    "vbl",
    "pla",
    "st",
    "superl",
    "pp",
    "obs",
    "adjective",
    "pret",
    "dv",
    "pn",
    "vb",
    "exclam",
    "obj",
    "quant",
    "noun",
    "compar",
    "ads",
    "ad",
    "ind",
    "col",
    "ph",
    "ing",
    "verb",
    "fem",
    "imperative",
    "pr",
    "usu",
    "indef",
    "dat",
)
POS_MARKER_PATTERN = "|".join(POS_MARKERS)

# Some ECDICT rows store escaped newlines, then use WordNet-style one-letter
# POS markers without a dot, for example "\\nv be..." instead of "\nv. be...".
# Others hard-wrap one explanation across multiple indented lines. Normalize
# only known line-leading POS tokens, then join continuation lines back into
# the current explanation so wrapped prose does not become separate UI rows.
# A leading domain tag such as "[计]" is also a row marker because many ECDICT
# translation rows use it without an explicit POS prefix.
POS_MARKER_RE = re.compile(
    rf"(^|\n)({POS_MARKER_PATTERN})(?!\.)(\s+)",
    re.IGNORECASE,
)
LINE_START_POS_RE = re.compile(rf"^({POS_MARKER_PATTERN})\.\s+", re.IGNORECASE)
LINE_START_DOMAIN_TAG_RE = re.compile(r"^\[[^\]]+\]")


def sw_key(word: str) -> str:
    return "".join(ch for ch in word.lower() if ch.isalnum())


def as_int(value: str) -> int:
    try:
        return int(value or 0)
    except ValueError:
        return 0


def normalize_pos_markers(text: str) -> str:
    text = text or ""
    text = text.replace("\\r\\n", "\n")
    text = text.replace("\\n", "\n")
    text = text.replace("\\r", "\n")
    text = text.replace("\\t", " ")
    text = POS_MARKER_RE.sub(lambda match: f"{match.group(1)}{match.group(2)}.{match.group(3)}", text)

    normalized_lines: list[str] = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        if LINE_START_POS_RE.match(line) or LINE_START_DOMAIN_TAG_RE.match(line) or not normalized_lines:
            normalized_lines.append(line)
        else:
            normalized_lines[-1] = f"{normalized_lines[-1]} {line}"

    return "\n".join(normalized_lines)


def import_csv(source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    conn = sqlite3.connect(output)
    try:
        conn.execute("""
            CREATE TABLE entries (
                word TEXT PRIMARY KEY,
                sw TEXT NOT NULL,
                phonetic TEXT,
                definition TEXT,
                translation TEXT,
                bnc INTEGER,
                frq INTEGER,
                exchange TEXT
            ) WITHOUT ROWID
        """)
        conn.execute("CREATE INDEX entries_sw_idx ON entries(sw)")

        with source.open("r", encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            rows = []
            for row in reader:
                word = (row.get("word") or "").strip()
                if not word:
                    continue

                rows.append((
                    word,
                    sw_key(row.get("sw") or word),
                    row.get("phonetic") or "",
                    normalize_pos_markers(row.get("definition") or ""),
                    normalize_pos_markers(row.get("translation") or ""),
                    as_int(row.get("bnc") or ""),
                    as_int(row.get("frq") or ""),
                    row.get("exchange") or "",
                ))

                if len(rows) >= 5000:
                    conn.executemany("""
                        INSERT OR REPLACE INTO entries
                        (word, sw, phonetic, definition, translation, bnc, frq, exchange)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    """, rows)
                    rows.clear()

            if rows:
                conn.executemany("""
                    INSERT OR REPLACE INTO entries
                    (word, sw, phonetic, definition, translation, bnc, frq, exchange)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """, rows)

        conn.commit()
    finally:
        conn.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert ECDICT CSV to Swan Dict SQLite database.")
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("third_party/ECDICT/ecdict.csv"),
        help="Path to ECDICT CSV input.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("applet/contents/data/ecdict.sqlite"),
        help="Path to generated SQLite database.",
    )
    args = parser.parse_args()
    import_csv(args.source, args.output)


if __name__ == "__main__":
    main()
