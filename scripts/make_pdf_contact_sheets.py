#!/usr/bin/env python3
"""Create contact sheets from rendered handbook pages for visual QA."""

from pathlib import Path

from PIL import Image, ImageDraw


render_dir = Path(__file__).resolve().parents[1] / "output" / "pdf-render"
pages = sorted(
    render_dir.glob("page-*.png"),
    key=lambda path: int(path.stem.split("-")[-1]),
)
thumb_width, thumb_height = 207, 292
columns, rows = 4, 5
batch_size = columns * rows

for batch_index in range((len(pages) + batch_size - 1) // batch_size):
    group = pages[batch_index * batch_size : (batch_index + 1) * batch_size]
    canvas = Image.new(
        "RGB",
        (columns * thumb_width, rows * (thumb_height + 18)),
        "#d9dee2",
    )
    draw = ImageDraw.Draw(canvas)
    for item_index, page_path in enumerate(group):
        page = Image.open(page_path).convert("RGB")
        page.thumbnail((thumb_width, thumb_height))
        x = (item_index % columns) * thumb_width + (thumb_width - page.width) // 2
        y = (item_index // columns) * (thumb_height + 18)
        canvas.paste(page, (x, y))
        draw.text((x + 4, y + thumb_height + 2), page_path.stem.split("-")[-1], fill="black")
    canvas.save(render_dir / f"contact-{batch_index + 1}.jpg", quality=88)

print(f"{len(pages)} pages, {(len(pages) + batch_size - 1) // batch_size} contact sheets")
