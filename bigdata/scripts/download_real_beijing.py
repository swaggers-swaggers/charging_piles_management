#!/usr/bin/env python3
"""Download and verify the public Beijing charging transaction release.

The large Parquet files are intentionally ignored by Git.  This script is the
reproducible hand-off: it resumes partial downloads and verifies every MD5
published by Figshare before writing the local source manifest.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


ARTICLE_ID = 31952289
API_URL = f"https://api.figshare.com/v2/articles/{ARTICLE_ID}"
EXPECTED = {
    "code.ipynb": (63_182, "2374b6348675f71b3f9e3857eee5dafd"),
    "stations_public.parquet": (150_533, "449a2adaa391895d4c29944a6e400794"),
    "orders_2025-01_public.parquet": (58_178_602, "c66ee6b6149fba215bc6b88667af2d18"),
    "orders_2025-07_public.parquet": (189_701_623, "64a4e6df4e5dbcc06040e650737f7b0a"),
}


def md5(path: Path) -> str:
    digest = hashlib.md5()  # nosec B324 - integrity value supplied by publisher
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, path: Path, expected_size: int) -> None:
    if expected_size >= 50_000_000:
        parallel_download(url, path, expected_size)
        return
    start = path.stat().st_size if path.exists() else 0
    if start > expected_size:
        path.unlink()
        start = 0
    headers = {"Range": f"bytes={start}-"} if start else {}
    request = urllib.request.Request(url, headers=headers)
    response = urllib.request.urlopen(request, timeout=120)
    resumed = start > 0 and response.status == 206
    mode = "ab" if resumed else "wb"
    with response, path.open(mode) as output:
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            output.write(block)


def parallel_download(url: str, path: Path, expected_size: int, workers: int = 8) -> None:
    part_dir = path.parent / (path.name + ".parts")
    part_dir.mkdir(parents=True, exist_ok=True)
    chunk = (expected_size + workers - 1) // workers

    def fetch(index: int) -> Path:
        start = index * chunk
        end = min(expected_size - 1, start + chunk - 1)
        target = part_dir / f"part-{index:02d}"
        required = end - start + 1
        if target.exists() and target.stat().st_size == required:
            return target
        for attempt in range(5):
            try:
                request = urllib.request.Request(url, headers={"Range": f"bytes={start}-{end}"})
                with urllib.request.urlopen(request, timeout=180) as response:
                    if response.status != 206:
                        raise RuntimeError(f"Range request returned HTTP {response.status}")
                    temporary = target.with_suffix(".tmp")
                    with temporary.open("wb") as output:
                        while True:
                            block = response.read(1024 * 1024)
                            if not block:
                                break
                            output.write(block)
                    if temporary.stat().st_size != required:
                        raise RuntimeError(f"short range {index}: {temporary.stat().st_size}/{required}")
                    temporary.replace(target)
                    return target
            except Exception:
                if attempt == 4:
                    raise
        raise AssertionError("unreachable")

    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(fetch, index) for index in range(workers)]
        for future in as_completed(futures):
            part = future.result()
            print(json.dumps({"downloaded_part": part.name,
                              "bytes": part.stat().st_size}), flush=True)
    temporary = path.with_suffix(path.suffix + ".assembling")
    with temporary.open("wb") as output:
        for index in range(workers):
            with (part_dir / f"part-{index:02d}").open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    output.write(block)
    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=".bigdata/source/real/beijing_figshare")
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(API_URL, timeout=60) as response:
        article = json.load(response)
    published = {row["name"]: row for row in article["files"]}
    files = []
    for name, (size, checksum) in EXPECTED.items():
        remote = published.get(name)
        if not remote or int(remote["size"]) != size or remote["computed_md5"] != checksum:
            raise RuntimeError(f"Publisher metadata changed for {name}; review before using it")
        path = output / name
        if not path.exists() or path.stat().st_size != size or md5(path) != checksum:
            download(remote["download_url"], path, size)
        actual = md5(path)
        if path.stat().st_size != size or actual != checksum:
            raise RuntimeError(f"Integrity verification failed for {name}")
        files.append({"name": name, "bytes": size, "md5": actual,
                      "download_url": remote["download_url"]})
        print(json.dumps({"verified": name, "bytes": size}), flush=True)
    manifest = {
        "dataset": "A city-scale dataset of public electric vehicle charging transactions and infrastructure",
        "city": "Beijing, China",
        "publisher": "Figshare; derived from Beijing charging-station operators",
        "doi": article["doi"],
        "license": article["license"]["name"],
        "article_url": article["figshare_url"],
        "periods": ["2025-01", "2025-07"],
        "reported_transactions": 8_544_000,
        "reported_stations": 8_553,
        "downloaded_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "files": files,
    }
    temporary = output / "manifest.json.part"
    temporary.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    temporary.replace(output / "manifest.json")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
