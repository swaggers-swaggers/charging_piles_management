#!/usr/bin/env python3
"""Download the official City of Boulder EV charging transaction table.

The ArcGIS service caps a response at 1,000 records.  Each page is stored as
gzip JSON Lines so interrupted downloads can resume without restarting.
"""
from __future__ import annotations

import argparse
import datetime as dt
import gzip
import hashlib
import json
import os
import time
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


SERVICE = (
    "https://services.arcgis.com/ePKBjXrBZ2vEEgWd/arcgis/rest/services/"
    "Electric_Vehicle_Charging_Station_Data/FeatureServer/0"
)
METADATA = (
    "https://www.arcgis.com/sharing/rest/content/items/"
    "95992b3938be4622b07f0b05eba95d4c/info/metadata/metadata.xml"
    "?format=default&output=html"
)
PAGE_SIZE = 1000


def request_json(path: str, params: dict[str, object], attempts: int = 5) -> dict:
    url = SERVICE + path + "?" + urllib.parse.urlencode(params)
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=60) as response:
                value = json.load(response)
            if "error" in value:
                raise RuntimeError(value["error"])
            return value
        except Exception:
            if attempt + 1 == attempts:
                raise
            time.sleep(2 ** attempt)
    raise AssertionError("unreachable")


def valid_page(path: Path, expected: int) -> bool:
    if not path.exists():
        return False
    try:
        with gzip.open(path, "rt", encoding="utf-8") as stream:
            return sum(1 for _ in stream) == expected
    except (OSError, EOFError):
        return False


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=".bigdata/source/real/boulder")
    parser.add_argument("--workers", type=int, default=8)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    pages = output / "pages"
    pages.mkdir(parents=True, exist_ok=True)

    count = int(request_json("/query", {
        "where": "1=1", "returnCountOnly": "true", "f": "json"
    })["count"])
    def download_page(offset: int) -> int:
        expected = min(PAGE_SIZE, count - offset)
        page = pages / f"part-{offset:09d}.jsonl.gz"
        if not valid_page(page, expected):
            payload = request_json("/query", {
                "where": "1=1",
                "outFields": "*",
                "returnGeometry": "false",
                "orderByFields": "ObjectId2",
                "resultOffset": offset,
                "resultRecordCount": PAGE_SIZE,
                "f": "json",
            })
            records = [row["attributes"] for row in payload.get("features", [])]
            if len(records) != expected:
                raise RuntimeError(
                    f"page {offset}: expected {expected} rows, received {len(records)}"
                )
            temporary = page.with_suffix(page.suffix + ".part")
            with gzip.open(temporary, "wt", encoding="utf-8") as stream:
                for record in records:
                    stream.write(json.dumps(record, ensure_ascii=False) + "\n")
            os.replace(temporary, page)
        return expected

    downloaded = 0
    offsets = list(range(0, count, PAGE_SIZE))
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        futures = [pool.submit(download_page, offset) for offset in offsets]
        for future in as_completed(futures):
            downloaded += future.result()
            if downloaded % 10000 == 0 or downloaded == count:
                print(json.dumps({"downloaded": downloaded, "total": count}), flush=True)

    digest = hashlib.sha256()
    size = 0
    for page in sorted(pages.glob("part-*.jsonl.gz")):
        content = page.read_bytes()
        digest.update(content)
        size += len(content)
    manifest = {
        "dataset": "City of Boulder Electric Vehicle Charging Station Data",
        "publisher": "City of Boulder",
        "license": "CC0 1.0 Public Domain",
        "metadata_url": METADATA,
        "api_url": SERVICE,
        "downloaded_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "record_count": count,
        "page_count": len(list(pages.glob("part-*.jsonl.gz"))),
        "compressed_bytes": size,
        "ordered_pages_sha256": digest.hexdigest(),
        "format": "gzip JSON Lines; one charging transaction per line",
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
