#!/usr/bin/env python3
"""Validate handbook structure, mappings, local links, and a seeded 20-row audit."""

from __future__ import annotations

import random
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HANDBOOK = ROOT / "docs" / "interview-handbook"
MAPPING = HANDBOOK / "07-question-mapping.md"
PDF = ROOT / "output" / "Five-Project-Embedded-Interview-Handbook.pdf"
REQUIRED = ["README.md"] + [
    f"{index:02d}-{name}.md"
    for index, name in enumerate(
        [
            "c-cpp-memory",
            "arm-freertos",
            "linux-process-thread",
            "linux-io-network",
            "linux-driver",
            "project-stories",
            "question-mapping",
            "follow-up-tree",
            "experiments",
            "flashcards",
            "glossary",
        ],
        start=1,
    )
]
CHAPTERS = {f"{index:02d}" for index in range(1, 12)}


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


for filename in REQUIRED:
    if not (HANDBOOK / filename).is_file():
        fail(f"missing handbook source: {filename}")

sources = [HANDBOOK / filename for filename in REQUIRED]
all_text = "\n".join(path.read_text(encoding="utf-8") for path in sources)
if len(all_text) < 100_000:
    fail(f"handbook is unexpectedly short: {len(all_text)} characters")

for badge in ("CODE", "HOST", "CROSS", "HIL", "TODO-HIL", "RELATED", "BASE"):
    if badge not in all_text:
        fail(f"missing evidence marker: {badge}")

rows = []
for line in MAPPING.read_text(encoding="utf-8").splitlines():
    match = re.match(r"^\|\s*(\d+)\s*\|\s*(.*?)\s*\|\s*([ABC])\s*\|\s*(.*?)\s*\|$", line)
    if match:
        number, topic, level, target = match.groups()
        rows.append((int(number), topic, level, target))

numbers = [row[0] for row in rows]
if numbers != list(range(1, 217)):
    missing = sorted(set(range(1, 217)) - set(numbers))
    duplicates = sorted(number for number in set(numbers) if numbers.count(number) > 1)
    fail(f"mapping must contain ordered 1..216; missing={missing}, duplicates={duplicates}")

counts = {level: sum(row[2] == level for row in rows) for level in "ABC"}
if counts != {"A": 76, "B": 99, "C": 41}:
    fail(f"unexpected tier counts: {counts}")

chapter_errors = []
for number, _, _, target in rows:
    mentioned = re.findall(r"(?<!\d)(0[1-9]|1[01])(?:[-：]|\b)", target)
    if not mentioned or any(chapter not in CHAPTERS for chapter in mentioned):
        chapter_errors.append((number, target))
if chapter_errors:
    fail(f"mapping rows without a valid chapter reference: {chapter_errors[:5]}")

link_errors = []
for source in sources + [ROOT / "README.md", ROOT / "docs" / "FIVE_PROJECT_INTERVIEW_HANDBOOK.md"]:
    text = source.read_text(encoding="utf-8")
    for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", text):
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        path_part = target.split("#", 1)[0]
        if path_part and not (source.parent / path_part).resolve().exists():
            link_errors.append(f"{source.relative_to(ROOT)} -> {target}")
if link_errors:
    fail("broken local links:\n" + "\n".join(link_errors))

story = (HANDBOOK / "06-project-stories.md").read_text(encoding="utf-8")
for project in (
    "Handheld_Temperature_STM32F103C8T6",
    "Smart_Medicine_Cart_STM32F103C8T6",
    "EmbeddedLinux_3DLiDAR_Perception",
    "EmbeddedLinux_MultiSource_OpticalDataFusion",
    "EmbeddedLinux_IMX6ULL_ConditionMonitor",
):
    if f"github.com/Vikebt/{project}" not in story:
        fail(f"project story lacks GitHub link for {project}")
if "study-step-" not in story:
    fail("project story lacks fixed study-step links")

if not PDF.is_file() or PDF.stat().st_size < 100_000:
    fail("final PDF is missing or unexpectedly small")

rng = random.Random(20261002)
sample = sorted(rng.sample(rows, 20), key=lambda row: row[0])
print(f"PASS: {len(rows)} mappings; tiers={counts}; chars={len(all_text)}; PDF={PDF.stat().st_size} bytes")
print("Seeded 20-row mapping audit:")
for number, topic, level, target in sample:
    print(f"  {number:03d} [{level}] {topic} -> {target}")
