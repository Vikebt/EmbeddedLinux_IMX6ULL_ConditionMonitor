#!/usr/bin/env python3
"""Build the five-project interview handbook PDF from canonical Markdown files."""

from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    HRFlowable,
    KeepTogether,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "docs" / "interview-handbook"
OUTPUT = ROOT / "output" / "Five-Project-Embedded-Interview-Handbook.pdf"
FONT_PATH = Path(r"C:\Windows\Fonts\simhei.ttf")

SOURCE_FILES = [
    "README.md",
    "01-c-cpp-memory.md",
    "02-arm-freertos.md",
    "03-linux-process-thread.md",
    "04-linux-io-network.md",
    "05-linux-driver.md",
    "06-project-stories.md",
    "07-question-mapping.md",
    "08-follow-up-tree.md",
    "09-experiments.md",
    "10-flashcards.md",
    "11-glossary.md",
]


def register_fonts() -> None:
    if not FONT_PATH.exists():
        raise FileNotFoundError(f"Chinese font not found: {FONT_PATH}")
    pdfmetrics.registerFont(TTFont("CN", str(FONT_PATH)))
    pdfmetrics.registerFont(TTFont("CN-Bold", str(FONT_PATH)))
    pdfmetrics.registerFont(TTFont("CN-Mono", str(FONT_PATH)))


def inline(text: str) -> str:
    """Convert the small Markdown inline subset used by the handbook."""
    placeholders: list[str] = []

    def stash(value: str) -> str:
        placeholders.append(value)
        return f"@@INLINE{len(placeholders) - 1}@@"

    text = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        lambda m: stash(
            f'<link href="{html.escape(m.group(2), quote=True)}" '
            f'color="#165D8C">{html.escape(m.group(1))}</link>'
        ),
        text,
    )
    text = re.sub(
        r"`([^`]+)`",
        lambda m: stash(f'<font name="CN-Mono" color="#8B3A3A">{html.escape(m.group(1))}</font>'),
        text,
    )
    escaped = html.escape(text)
    escaped = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", escaped)
    escaped = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<i>\1</i>", escaped)
    for index, value in enumerate(placeholders):
        escaped = escaped.replace(f"@@INLINE{index}@@", value)
    return escaped


class HandbookDocTemplate(BaseDocTemplate):
    def __init__(self, filename: str, **kwargs):
        super().__init__(filename, **kwargs)
        self._heading_seq = 0

    def beforeDocument(self):
        # multiBuild lays the document out repeatedly until the TOC stabilizes.
        # Bookmark keys must therefore be deterministic on every pass.
        self._heading_seq = 0

    def afterFlowable(self, flowable):
        if not isinstance(flowable, Paragraph):
            return
        level = getattr(flowable, "toc_level", None)
        if level is None:
            return
        self._heading_seq += 1
        key = f"heading-{self._heading_seq}"
        self.canv.bookmarkPage(key)
        self.canv.addOutlineEntry(flowable.getPlainText(), key, level=level, closed=level > 0)
        self.notify("TOCEntry", (level, flowable.getPlainText(), self.page, key))


def build_styles():
    base = getSampleStyleSheet()
    styles = {
        "body": ParagraphStyle(
            "BodyCN",
            parent=base["BodyText"],
            fontName="CN",
            fontSize=11.0,
            leading=17.8,
            textColor=colors.HexColor("#24313A"),
            alignment=TA_JUSTIFY,
            spaceAfter=3.2,
            allowWidows=0,
            allowOrphans=0,
        ),
        "h1": ParagraphStyle(
            "H1CN",
            parent=base["Heading1"],
            fontName="CN-Bold",
            fontSize=19,
            leading=25,
            textColor=colors.HexColor("#123B56"),
            spaceBefore=8,
            spaceAfter=11,
            keepWithNext=True,
        ),
        "h2": ParagraphStyle(
            "H2CN",
            parent=base["Heading2"],
            fontName="CN-Bold",
            fontSize=14,
            leading=19,
            textColor=colors.HexColor("#176B87"),
            spaceBefore=10,
            spaceAfter=6,
            keepWithNext=True,
        ),
        "h3": ParagraphStyle(
            "H3CN",
            parent=base["Heading3"],
            fontName="CN-Bold",
            fontSize=11.2,
            leading=16,
            textColor=colors.HexColor("#845A17"),
            spaceBefore=7,
            spaceAfter=4,
            keepWithNext=True,
        ),
        "h4": ParagraphStyle(
            "H4CN",
            parent=base["Heading4"],
            fontName="CN-Bold",
            fontSize=9.8,
            leading=14,
            textColor=colors.HexColor("#4C5A63"),
            spaceBefore=5,
            spaceAfter=3,
            keepWithNext=True,
        ),
        "list": ParagraphStyle(
            "ListCN",
            parent=base["BodyText"],
            fontName="CN",
            fontSize=10.5,
            leading=17.0,
            textColor=colors.HexColor("#24313A"),
            leftIndent=13,
            firstLineIndent=-8,
            spaceAfter=2.3,
        ),
        "quote": ParagraphStyle(
            "QuoteCN",
            parent=base["BodyText"],
            fontName="CN",
            fontSize=10.3,
            leading=16.7,
            leftIndent=11,
            rightIndent=7,
            borderColor=colors.HexColor("#4B9AB5"),
            borderWidth=1.5,
            borderPadding=(5, 7, 5, 8),
            backColor=colors.HexColor("#EEF6F8"),
            textColor=colors.HexColor("#27434F"),
            spaceBefore=4,
            spaceAfter=6,
        ),
        "code": ParagraphStyle(
            "CodeCN",
            parent=base["Code"],
            fontName="CN-Mono",
            fontSize=8.1,
            leading=12.2,
            leftIndent=7,
            rightIndent=7,
            borderColor=colors.HexColor("#CCD5DA"),
            borderWidth=0.5,
            borderPadding=6,
            backColor=colors.HexColor("#F4F6F7"),
            textColor=colors.HexColor("#25333B"),
            spaceBefore=4,
            spaceAfter=6,
            splitLongWords=True,
        ),
        "table": ParagraphStyle(
            "TableCN",
            parent=base["BodyText"],
            fontName="CN",
            fontSize=8.0,
            leading=12.0,
            textColor=colors.HexColor("#263238"),
        ),
        "tocTitle": ParagraphStyle(
            "TocTitleCN",
            parent=base["Heading1"],
            fontName="CN-Bold",
            fontSize=21,
            leading=28,
            textColor=colors.HexColor("#123B56"),
            spaceAfter=14,
        ),
    }
    return styles


def page_number(canvas, doc):
    canvas.saveState()
    page = canvas.getPageNumber()
    canvas.setStrokeColor(colors.HexColor("#D7DEE2"))
    canvas.line(21 * mm, 16.5 * mm, 189 * mm, 16.5 * mm)
    canvas.setFont("CN", 7.5)
    canvas.setFillColor(colors.HexColor("#66747C"))
    canvas.drawString(21 * mm, 11.5 * mm, "五项目结合式嵌入式面试补充讲义")
    canvas.drawRightString(189 * mm, 11.5 * mm, f"{page}")
    if page > 2:
        canvas.setFont("CN", 7.2)
        canvas.setFillColor(colors.HexColor("#7A878E"))
        canvas.drawString(21 * mm, 284 * mm, "C/C++ · FreeRTOS · Linux · 驱动 · 项目证据")
    canvas.restoreState()


def cover_story(styles):
    story = [Spacer(1, 26 * mm)]
    story.append(
        Paragraph(
            "五项目结合式<br/>嵌入式面试补充讲义",
            ParagraphStyle(
                "CoverTitle",
                fontName="CN-Bold",
                fontSize=29,
                leading=40,
                alignment=TA_CENTER,
                textColor=colors.HexColor("#123B56"),
            ),
        )
    )
    story.append(Spacer(1, 8 * mm))
    story.append(HRFlowable(width="55%", thickness=2, color=colors.HexColor("#2F89A8")))
    story.append(Spacer(1, 8 * mm))
    story.append(
        Paragraph(
            "从 216 道基础题到五个真实代码仓库的机制、证据、取舍与验证",
            ParagraphStyle(
                "CoverSub",
                fontName="CN",
                fontSize=13,
                leading=21,
                alignment=TA_CENTER,
                textColor=colors.HexColor("#40545F"),
            ),
        )
    )
    story.append(Spacer(1, 20 * mm))
    cards = [
        ["P1", "手持体温检测仪", "FreeRTOS · 融合 · CRC"],
        ["P2", "智能送药小车", "ISR · FSM · Tick"],
        ["P3", "三维 LiDAR 感知", "UDP · epoll · 有界队列"],
        ["P4", "多源光电融合", "线程 · 快照 · RAII"],
        ["P5", "I.MX6ULL 状态监测器", "DTS · SPI · IIO"],
    ]
    card_data = [[Paragraph(f"<b>{a}</b>", styles["body"]), Paragraph(b, styles["body"]), Paragraph(c, styles["body"])] for a, b, c in cards]
    table = Table(card_data, colWidths=[18 * mm, 60 * mm, 72 * mm], hAlign="CENTER")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (0, -1), colors.HexColor("#176B87")),
                ("TEXTCOLOR", (0, 0), (0, -1), colors.white),
                ("BACKGROUND", (1, 0), (-1, -1), colors.HexColor("#F2F7F9")),
                ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#B8C7CE")),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 7),
                ("RIGHTPADDING", (0, 0), (-1, -1), 7),
                ("TOPPADDING", (0, 0), (-1, -1), 7),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
            ]
        )
    )
    story.append(table)
    story.append(Spacer(1, 22 * mm))
    story.append(
        Paragraph(
            "学习版 · 面试版 · 实验版<br/>证据等级：CODE / HOST / CROSS / HIL / TODO-HIL",
            ParagraphStyle(
                "CoverMeta",
                fontName="CN",
                fontSize=10,
                leading=17,
                alignment=TA_CENTER,
                textColor=colors.HexColor("#65747C"),
            ),
        )
    )
    story.append(PageBreak())
    story.append(Paragraph("目录", styles["tocTitle"]))
    toc = TableOfContents()
    toc.levelStyles = [
        ParagraphStyle("TOC1", fontName="CN-Bold", fontSize=9.5, leading=15, leftIndent=0, firstLineIndent=0, textColor=colors.HexColor("#123B56")),
        ParagraphStyle("TOC2", fontName="CN", fontSize=8.2, leading=12.2, leftIndent=12, firstLineIndent=0, textColor=colors.HexColor("#42545E")),
        ParagraphStyle("TOC3", fontName="CN", fontSize=7.3, leading=10.5, leftIndent=24, firstLineIndent=0, textColor=colors.HexColor("#65747C")),
    ]
    story.append(toc)
    story.append(PageBreak())
    return story


def table_flowable(rows, styles, available_width):
    if not rows:
        return None
    width_count = max(len(row) for row in rows)
    normalized = [row + [""] * (width_count - len(row)) for row in rows]
    data = [[Paragraph(inline(cell.strip()), styles["table"]) for cell in row] for row in normalized]
    if width_count == 2:
        widths = [available_width * 0.25, available_width * 0.75]
    elif width_count == 3:
        widths = [available_width * 0.18, available_width * 0.36, available_width * 0.46]
    elif width_count == 4:
        widths = [available_width * 0.12, available_width * 0.23, available_width * 0.32, available_width * 0.33]
    else:
        widths = [available_width / width_count] * width_count
    table = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#DCECF1")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.HexColor("#123B56")),
                ("FONTNAME", (0, 0), (-1, 0), "CN-Bold"),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F7F9FA")]),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#BCC8CE")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 4.2),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4.2),
                ("TOPPADDING", (0, 0), (-1, -1), 4.2),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4.2),
            ]
        )
    )
    return table


def parse_markdown(path: Path, styles, available_width):
    lines = path.read_text(encoding="utf-8").splitlines()
    result = []
    paragraph: list[str] = []
    quote: list[str] = []
    code: list[str] = []
    table_rows: list[list[str]] = []
    in_code = False

    def flush_paragraph():
        nonlocal paragraph
        if paragraph:
            result.append(Paragraph(inline(" ".join(x.strip() for x in paragraph)), styles["body"]))
            paragraph = []

    def flush_quote():
        nonlocal quote
        if quote:
            result.append(Paragraph(inline(" ".join(x.strip() for x in quote)), styles["quote"]))
            quote = []

    def flush_code():
        nonlocal code
        if code:
            escaped = "<br/>".join(html.escape(x).replace(" ", "&nbsp;") for x in code)
            result.append(Paragraph(escaped or "&nbsp;", styles["code"]))
            code = []

    def flush_table():
        nonlocal table_rows
        if table_rows:
            cleaned = []
            for row in table_rows:
                if all(re.fullmatch(r"\s*:?-{3,}:?\s*", cell or "") for cell in row):
                    continue
                cleaned.append(row)
            if cleaned:
                result.append(table_flowable(cleaned, styles, available_width))
                result.append(Spacer(1, 5))
            table_rows = []

    for raw in lines:
        line = raw.rstrip()
        if line.startswith("```"):
            flush_paragraph(); flush_quote(); flush_table()
            if in_code:
                flush_code()
                in_code = False
            else:
                in_code = True
            continue
        if in_code:
            code.append(line)
            continue
        if line.startswith("|") and line.endswith("|"):
            flush_paragraph(); flush_quote()
            table_rows.append([cell.strip() for cell in line.strip("|").split("|")])
            continue
        flush_table()
        heading = re.match(r"^(#{1,4})\s+(.+)$", line)
        if heading:
            flush_paragraph(); flush_quote()
            level = len(heading.group(1))
            p = Paragraph(inline(heading.group(2)), styles[f"h{level}"])
            # Keep the printed TOC at chapter/section depth. Including every
            # topic card (H3) creates hundreds of entries whose own reflow
            # shifts later page numbers across too many multiBuild passes.
            if level <= 2:
                p.toc_level = level - 1
            result.append(p)
            continue
        if line.startswith(">"):
            flush_paragraph()
            quote.append(line[1:].strip())
            continue
        flush_quote()
        bullet = re.match(r"^\s*[-*]\s+(.+)$", line)
        ordered = re.match(r"^\s*(\d+)\.\s+(.+)$", line)
        check = re.match(r"^\s*-\s+\[([ xX])\]\s+(.+)$", line)
        if check:
            flush_paragraph()
            mark = "☑" if check.group(1).lower() == "x" else "☐"
            result.append(Paragraph(f"{mark}&nbsp;&nbsp;{inline(check.group(2))}", styles["list"]))
        elif bullet:
            flush_paragraph()
            result.append(Paragraph(f"•&nbsp;&nbsp;{inline(bullet.group(1))}", styles["list"]))
        elif ordered:
            flush_paragraph()
            result.append(Paragraph(f"{ordered.group(1)}.&nbsp;&nbsp;{inline(ordered.group(2))}", styles["list"]))
        elif not line.strip():
            flush_paragraph()
        elif re.fullmatch(r"-{3,}", line.strip()):
            flush_paragraph()
            result.append(HRFlowable(width="100%", thickness=0.5, color=colors.HexColor("#CBD5DA"), spaceBefore=4, spaceAfter=4))
        else:
            paragraph.append(line)
    flush_paragraph(); flush_quote(); flush_code(); flush_table()
    return result


def build() -> None:
    register_fonts()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    page_width, page_height = A4
    left = right = 22.5 * mm
    top = 20 * mm
    bottom = 22 * mm
    available_width = page_width - left - right
    frame = Frame(left, bottom, available_width, page_height - top - bottom, id="normal")
    template = PageTemplate(id="handbook", frames=[frame], onPage=page_number)
    doc = HandbookDocTemplate(
        str(OUTPUT),
        pagesize=A4,
        leftMargin=left,
        rightMargin=right,
        topMargin=top,
        bottomMargin=bottom,
        title="五项目结合式嵌入式面试补充讲义",
        author="Vikebt",
        subject="C/C++、FreeRTOS、Linux 应用与 Linux 驱动项目化面试讲义",
    )
    doc.addPageTemplates([template])
    styles = build_styles()
    story = cover_story(styles)
    for index, filename in enumerate(SOURCE_FILES):
        if index:
            story.append(PageBreak())
        story.extend(parse_markdown(SOURCE_DIR / filename, styles, available_width))
    doc.multiBuild(story)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    build()
