#!/usr/bin/env python3
import argparse
import csv
import sqlite3
from pathlib import Path


def sw_key(word: str) -> str:
    return "".join(ch for ch in word.lower() if ch.isalnum())


def as_int(value: str) -> int:
    try:
        return int(value or 0)
    except ValueError:
        return 0


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
                    row.get("definition") or "",
                    row.get("translation") or "",
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
