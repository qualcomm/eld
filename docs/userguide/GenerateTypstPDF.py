#!/usr/bin/env python3
#"===----------------------------------------------------------------------===
# Part of the eld Project, under the BSD License
# See https://github.com/qualcomm/eld/LICENSE.txt for license information.
# SPDX-License-Identifier: BSD-3-Clause
#"===----------------------------------------------------------------------===

"""Generate a Typst based formatted PDF."""

from __future__ import annotations

import argparse
import base64
import binascii
import io
import pathlib
import re
import subprocess

from lxml import html
from PIL import Image
from pygments import highlight
from pygments.formatter import Formatter
from pygments.lexers import get_lexer_by_name, guess_lexer
from pygments.styles import get_style_by_name
from pygments.token import STANDARD_TYPES, Token
from pygments.util import ClassNotFound


BLOCK_TAGS = {
    "address",
    "article",
    "aside",
    "blockquote",
    "body",
    "caption",
    "center",
    "dd",
    "details",
    "dialog",
    "dir",
    "div",
    "dl",
    "dt",
    "fieldset",
    "figcaption",
    "figure",
    "footer",
    "form",
    "h1",
    "h2",
    "h3",
    "h4",
    "h5",
    "h6",
    "h7",
    "h8",
    "header",
    "hr",
    "html",
    "legend",
    "li",
    "main",
    "nav",
    "ol",
    "p",
    "pre",
    "section",
    "table",
    "tbody",
    "td",
    "tfoot",
    "th",
    "thead",
    "tr",
    "ul",
}
SKIP_CLASSES = {"headerlink", "viewcode-link"}
SKIP_TAGS = {"script", "style", "meta", "link"}
# A4 page width minus the horizontal margins configured in the Typst template.
PAGE_TEXT_WIDTH_PT = (8.27 - 1.5) * 72
MAX_IMAGE_HEIGHT_IN = 8.8
MAX_IMAGE_HEIGHT_PT = MAX_IMAGE_HEIGHT_IN * 72
ADMONITION_TITLES = {
    "admonition-title",
}
EMBEDDED_GIF_DATA_RE = re.compile(
    r'(?P<attr>(?:xlink:)?href=)(?P<quote>["\'])'
    r'(?P<data_url>data:image/gif;base64,(?P<encoded>[^"\']+))(?P=quote)'
)
GIF_BACKGROUND_RGBA = (255, 255, 255, 255)
PDF_LABEL_ATTR = "data-eld-pdf-labels"
INDEX_REFERENCE_RE = re.compile(r"^\[\d+\]$")
PYGMENTS_DEFAULT_STYLE = get_style_by_name("default")
# Sphinx/Pygments HTML output annotates tokens with compact CSS class names,
# such as ``k`` for keywords and ``s`` for strings. Reverse Pygments'
# STANDARD_TYPES mapping so the converter can recover the original token type
# from an already-highlighted ``<span class="...">`` node instead of lexing the
# code a second time.
PYGMENTS_CLASS_TOKENS = {
    class_name: token
    for token, class_name in STANDARD_TYPES.items()
    if class_name
}


def typst_string(text: str) -> str:
    return (
        '"'
        + text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
        + '"'
    )


def escape_text(text: str) -> str:
    if not text:
        return ""
    text = text.replace("\xa0", " ")
    text = re.sub(r"([\\#$%&~_^*\[\]<>`@])", r"\\\1", text)
    return text.replace("//", r"\/\/")


def clean_text(text: str) -> str:
    return re.sub(r"\s+", " ", text.replace("\xa0", " ")).strip()


def typst_code_style(style: dict[str, str]) -> str:
    # ``style_for_token`` returns a full Pygments style dictionary, but Typst
    # only needs the pieces that affect code highlighting in this template.
    # Convert the supported attributes to a composable ``#text(...)`` wrapper
    # and leave unsupported options (for example underlines or borders) out so
    # generated Typst stays simple and predictable.
    options = []
    if style["color"]:
        options.append(f'fill: rgb("#{style["color"]}")')
    if style["bold"]:
        options.append('weight: "bold"')
    if style["italic"]:
        options.append('style: "italic"')
    return f"#text({', '.join(options)})" if options else ""


def typst_code_token(text: str, token_type=Token.Text) -> str:
    if not text:
        return ""
    # Emit code as Typst raw text to preserve whitespace and punctuation
    # exactly. When the Pygments token has visual attributes, wrap that raw
    # fragment in a text style; otherwise return the raw fragment directly to
    # avoid producing unnecessary nested markup for plain text tokens.
    style = typst_code_style(PYGMENTS_DEFAULT_STYLE.style_for_token(token_type))
    raw = f"#raw({typst_string(text)})"
    return f"{style}[{raw}]" if style else raw


class TypstPygmentsFormatter(Formatter):
    def __init__(self, **options):
        super().__init__(style=PYGMENTS_DEFAULT_STYLE, **options)

    def format_unencoded(self, tokensource, outfile) -> None:
        # Pygments formatters normally emit HTML, LaTeX, or terminal escapes.
        # This custom formatter writes Typst fragments directly, one raw code
        # token at a time, so fallback lexing can share the exact same Typst
        # styling path as tokens recovered from Sphinx-generated HTML spans.
        for token_type, value in tokensource:
            outfile.write(typst_code_token(value, token_type))


def parse_svg_length(value: str | None) -> float | None:
    if not value:
        return None
    match = re.fullmatch(r"\s*([0-9.]+)\s*(?:pt|px)?\s*", value)
    return float(match.group(1)) if match else None


def is_skipped(element) -> bool:
    if not hasattr(element, "tag") or not isinstance(element.tag, str):
        return True
    if element.tag.lower() in SKIP_TAGS:
        return True
    classes = set((element.get("class") or "").split())
    return bool(classes & SKIP_CLASSES)


class TypstConverter:
    def __init__(self, input_html: pathlib.Path):
        self.input_html = input_html
        self._pdf_safe_gif_data_urls: dict[str, str] = {}
        self._href_labels: dict[str, str] = {}

    def href_target(self, href: str | None) -> tuple[str, str | None] | None:
        if not href or href.startswith(("http://", "https://", "mailto:")):
            return None
        path, separator, fragment = href.partition("#")
        if not path:
            return None
        path = path.removeprefix("./")
        if not path.endswith(".html"):
            return None
        document_name = path.removesuffix(".html")
        return document_name, fragment if separator else None

    def target_key(self, target: tuple[str, str | None]) -> str:
        document_name, fragment = target
        return f"{document_name}#{fragment}" if fragment else document_name

    def target_label(self, target: tuple[str, str | None]) -> str:
        return "pdf-" + re.sub(r"[^A-Za-z0-9_-]+", "-", self.target_key(target)).strip("-")

    def add_element_label(self, element, label: str) -> None:
        labels = (element.get(PDF_LABEL_ATTR) or "").split()
        if label not in labels:
            labels.append(label)
            element.set(PDF_LABEL_ATTR, " ".join(labels))

    def label_anchors(self, element, *, block: bool) -> str:
        labels = (element.get(PDF_LABEL_ATTR) or "").split()
        if not labels:
            return ""
        if block:
            return "".join(f"#metadata(none) <{label}>\n" for label in labels)
        return " ".join(f"#metadata(none) <{label}>" for label in labels) + " "

    def prepare_index_links(self, input_document) -> None:
        self._href_labels = {}
        for element in input_document.xpath(f"//*[@{PDF_LABEL_ATTR}]"):
            element.attrib.pop(PDF_LABEL_ATTR, None)

        index_path = self.index_html_path()
        if index_path is None:
            return

        index_document = html.parse(str(index_path))
        target_labels = {}
        href_targets = {}
        for anchor in index_document.xpath("//a[@href]"):
            href = anchor.get("href")
            target = self.href_target(href)
            if target is None:
                continue
            target_labels.setdefault(target, self.target_label(target))
            href_targets[self.target_key(target)] = target

        if not target_labels:
            return

        page_labels = {}
        fragment_labels = {}
        for target, label in target_labels.items():
            document_name, fragment = target
            if fragment is None:
                page_labels.setdefault(document_name, []).append(label)
            else:
                fragment_labels.setdefault((document_name, fragment), []).append(label)

        assigned_targets = set()
        current_document = "index"
        root = input_document.getroot()
        for label in page_labels.get("index", []):
            self.add_element_label(root, label)
            assigned_targets.add(("index", None))

        for element in input_document.xpath("//*"):
            element_id = element.get("id")
            if not element_id:
                continue

            tag = element.tag.lower() if isinstance(element.tag, str) else ""
            if tag == "span" and element_id.startswith("document-"):
                current_document = element_id.removeprefix("document-")
                for label in page_labels.get(current_document, []):
                    self.add_element_label(element, label)
                    assigned_targets.add((current_document, None))

            target = (current_document, element_id)
            if target not in assigned_targets:
                for label in fragment_labels.get(target, []):
                    self.add_element_label(element, label)
                    assigned_targets.add(target)

        for href_key, target in href_targets.items():
            if target in assigned_targets:
                self._href_labels[href_key] = target_labels[target]

    def internal_link_label(self, href: str | None) -> str | None:
        target = self.href_target(href)
        if target is None:
            return None
        return self._href_labels.get(self.target_key(target))

    def remove_index_section(self, document, heading_id: str) -> None:
        for heading in document.xpath(f'//h2[@id="{heading_id}"]'):
            parent = heading.getparent()
            sibling = heading.getnext()
            parent.remove(heading)
            while sibling is not None:
                next_sibling = sibling.getnext()
                tag = sibling.tag.lower() if isinstance(sibling.tag, str) else ""
                if tag == "h2":
                    break
                parent.remove(sibling)
                sibling = next_sibling

    def remove_index_heading(self, document, heading_id: str) -> None:
        for heading in document.xpath(f'//h2[@id="{heading_id}"]'):
            heading.getparent().remove(heading)

    def flatten_keyword_index_groups(self, document) -> None:
        for item in document.xpath(
            '//table[contains(concat(" ", normalize-space(@class), " "), " genindextable ")]//td/ul/li'
        ):
            if clean_text(item.text or "") != "command line option":
                continue
            child_lists = item.xpath("./ul")
            if len(child_lists) != 1:
                continue

            parent = item.getparent()
            index = parent.index(item)
            children = list(child_lists[0].xpath("./li"))
            parent.remove(item)
            for child in children:
                parent.insert(index, child)
                index += 1

    def normalize_index_references(self, document) -> None:
        for item in document.xpath(
            '//table[contains(concat(" ", normalize-space(@class), " "), " genindextable ")]//li'
        ):
            anchors = item.xpath("./a[@href]")
            keyword_anchor = next(
                (
                    anchor
                    for anchor in anchors
                    if not INDEX_REFERENCE_RE.fullmatch(clean_text(anchor.text_content()))
                ),
                None,
            )
            reference_anchors = [
                anchor
                for anchor in anchors
                if INDEX_REFERENCE_RE.fullmatch(clean_text(anchor.text_content()))
            ]
            if keyword_anchor is None or not reference_anchors:
                continue

            if self.internal_link_label(keyword_anchor.get("href")) is None:
                for anchor in reference_anchors:
                    if self.internal_link_label(anchor.get("href")) is not None:
                        keyword_anchor.set("href", anchor.get("href"))
                        break

            keyword_anchor.tail = ""
            for anchor in reference_anchors:
                item.remove(anchor)

    def prepare_pdf_index(self, document) -> None:
        self.remove_index_section(document, "Symbols")
        self.remove_index_heading(document, "C")
        self.flatten_keyword_index_groups(document)
        self.normalize_index_references(document)

    def svg_dimensions(self, source: str) -> tuple[float, float] | None:
        if not source.endswith(".svg"):
            return None
        source_path = (self.input_html.parent / source).resolve()
        if not source_path.is_file():
            return None
        match = re.search(
            r"<svg\b[^>]*\bwidth=\"([^\"]+)\"[^>]*\bheight=\"([^\"]+)\"",
            source_path.read_text(encoding="utf-8", errors="ignore"),
        )
        if not match:
            return None
        width = parse_svg_length(match.group(1))
        height = parse_svg_length(match.group(2))
        if width is None or height is None:
            return None
        return width, height

    def image_options(self, source: str) -> str:
        dimensions = self.svg_dimensions(source)
        if dimensions is None:
            return ", width: 100%"

        # Sphinx-generated SVG diagrams can be very tall when scaled to the
        # full text width. Cap those diagrams by height to keep them on a page.
        width, height = dimensions
        width_scale = PAGE_TEXT_WIDTH_PT / width
        if height * width_scale > MAX_IMAGE_HEIGHT_PT:
            return f", height: {MAX_IMAGE_HEIGHT_IN}in"
        return ", width: 100%"

    def pdf_safe_gif_data_url(self, encoded_image: str) -> str:
        data_url = self._pdf_safe_gif_data_urls.get(encoded_image)
        if data_url is not None:
            return data_url

        try:
            image_bytes = base64.b64decode(encoded_image, validate=True)
        except (binascii.Error, ValueError) as error:
            raise RuntimeError("Could not decode embedded image data") from error

        try:
            with Image.open(io.BytesIO(image_bytes)) as image:
                image.seek(0)
                frame = image.convert("RGBA")
        except Exception as error:
            raise RuntimeError("Could not convert embedded GIF image to JPEG") from error

        background = Image.new("RGBA", frame.size, GIF_BACKGROUND_RGBA)
        background.alpha_composite(frame)
        output = io.BytesIO()
        background.convert("RGB").save(output, format="JPEG")

        encoded_jpeg = base64.b64encode(output.getvalue()).decode("ascii")
        data_url = f"data:image/jpeg;base64,{encoded_jpeg}"
        self._pdf_safe_gif_data_urls[encoded_image] = data_url
        return data_url

    def pdf_safe_image_source(self, source: str) -> str:
        if not source.endswith(".svg"):
            return source

        source_path = (self.input_html.parent / source).resolve()
        if not source_path.is_file():
            return source

        svg = source_path.read_text(encoding="utf-8", errors="ignore")
        if "data:image/gif;base64" not in svg:
            return source

        def replace_gif(match: re.Match[str]) -> str:
            data_url = self.pdf_safe_gif_data_url(match.group("encoded"))
            return (
                f"{match.group('attr')}{match.group('quote')}"
                f"{data_url}{match.group('quote')}"
            )

        pdf_safe_svg = EMBEDDED_GIF_DATA_RE.sub(
            replace_gif,
            svg,
        )
        output_path = source_path.with_name(f"{source_path.stem}_pdf{source_path.suffix}")
        if not output_path.is_file() or output_path.read_text(
            encoding="utf-8", errors="ignore"
        ) != pdf_safe_svg:
            output_path.write_text(pdf_safe_svg, encoding="utf-8")

        return output_path.relative_to(self.input_html.parent).as_posix()

    def child_inline(self, element) -> str:
        parts = []
        if element.text:
            parts.append(escape_text(element.text))
        for child in element:
            parts.append(self.inline(child))
            if child.tail:
                parts.append(escape_text(child.tail))
        return "".join(parts)

    def inline(self, element) -> str:
        if is_skipped(element):
            return ""
        tag = element.tag.lower()
        if tag in {"strong", "b"}:
            return f"#strong[{self.child_inline(element)}]"
        if tag in {"em", "i"}:
            return f"#emph[{self.child_inline(element)}]"
        if tag in {"code", "tt", "kbd", "samp"}:
            return f"#text(fill: code-red)[#raw({typst_string(element.text_content())})]"
        if tag == "br":
            return "\\\n"
        if tag == "a":
            text = self.child_inline(element) or escape_text(clean_text(element.text_content()))
            href = element.get("href") or ""
            if href.startswith(("http://", "https://", "mailto:")):
                return f"#link({typst_string(href)})[{text}]"
            label = self.internal_link_label(href)
            if label is not None:
                return f"#link(<{label}>)[{text}]"
            return text
        classes = set((element.get("class") or "").split())
        if tag == "mark" or "highlighted" in classes:
            return f"#box(fill: highlight-yellow, inset: (x: 1pt, y: 0pt), radius: 1pt)[{self.child_inline(element)}]"
        if tag == "span":
            return self.label_anchors(element, block=False) + self.child_inline(element)
        if tag == "img":
            source = element.get("src")
            return self.image(source) if source else ""
        if tag in BLOCK_TAGS:
            return escape_text(clean_text(element.text_content()))
        return self.child_inline(element)

    def child_blocks(self, element) -> str:
        output = []
        if element.text and element.text.strip():
            output.append(escape_text(clean_text(element.text)))
            output.append("\n\n")
        for child in element:
            output.append(self.block(child))
            if child.tail and child.tail.strip():
                output.append(escape_text(clean_text(child.tail)))
                output.append("\n\n")
        return "".join(output)

    def block(self, element) -> str:
        if is_skipped(element):
            return ""
        tag = element.tag.lower()
        classes = set((element.get("class") or "").split())
        anchors = self.label_anchors(element, block=True)
        if classes & ADMONITION_TITLES:
            return anchors
        if "admonition" in classes:
            return anchors + self.admonition(element)
        if tag in {"h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8"}:
            level = int(tag[1:])
            heading = re.sub(r"\s+", " ", self.child_inline(element)).strip()
            if level == 1 and heading == "ELD User Guide":
                return anchors
            return f"{anchors}{'=' * level} {heading}\n\n"
        if tag == "p":
            content = self.child_inline(element).strip()
            return f"{anchors}{content}\n\n" if content else anchors
        if tag == "pre":
            return anchors + self.raw_block(element)
        if tag == "blockquote":
            body = self.child_blocks(element).strip()
            return f"{anchors}#quote(block: true)[\n{body}\n]\n\n" if body else anchors
        if tag in {"ul", "ol"}:
            return anchors + self.list_block(element, ordered=(tag == "ol"))
        if tag == "dl":
            return anchors + self.definition_list(element)
        if tag == "table":
            return anchors + self.table(element)
        if tag == "object":
            return anchors + self.figure(element.get("data"))
        if tag == "img":
            return anchors + self.figure(element.get("src"))
        if tag == "hr":
            return f"{anchors}#line(length: 100%)\n\n"
        if "contents" in classes and "topic" in classes:
            return anchors
        if "genindex-jumpbox" in classes:
            return anchors
        if tag in {"div", "section", "article", "main", "body"}:
            return anchors + self.child_blocks(element)
        if tag in {"dt", "dd", "li"}:
            content = self.child_inline(element).strip()
            return f"{anchors}{content}\n\n" if content else anchors + self.child_blocks(element)
        content = self.child_inline(element).strip()
        return f"{anchors}{content}\n\n" if content else anchors + self.child_blocks(element)

    def list_item_content(self, item) -> str:
        parts = []
        anchors = self.label_anchors(item, block=False).strip()
        if anchors:
            parts.append(anchors)
        if item.text and item.text.strip():
            parts.append(escape_text(clean_text(item.text)))
        for child in item:
            tag = child.tag.lower()
            if tag in {"ul", "ol"}:
                parts.append("\n" + self.block(child).rstrip())
            elif tag == "p":
                parts.append(self.child_inline(child).strip())
            else:
                rendered = self.block(child) if tag in BLOCK_TAGS else self.inline(child)
                parts.append(rendered.strip())
            if child.tail and child.tail.strip():
                parts.append(escape_text(clean_text(child.tail)))
        return "\n".join(part for part in parts if part).strip()

    def list_block(self, element, ordered: bool) -> str:
        marker = "+" if ordered else "-"
        lines = []
        for item in element.xpath("./li"):
            content = self.list_item_content(item)
            if not content:
                continue
            item_lines = content.splitlines()
            lines.append(f"{marker} {item_lines[0]}")
            lines.extend("  " + line for line in item_lines[1:])
        return "\n".join(lines) + ("\n\n" if lines else "")

    def definition_list(self, element) -> str:
        output = []
        children = list(element)
        child_index = 0
        while child_index < len(children):
            child = children[child_index]
            if child.tag.lower() == "dt":
                anchors = self.label_anchors(child, block=True)
                term = self.child_inline(child).strip() or escape_text(clean_text(child.text_content()))
                term = re.sub(r"(?:\\\s*)+$", "", term).strip()
                output.append(f"{anchors}#strong[{term}]\n")
                if child_index + 1 < len(children) and children[child_index + 1].tag.lower() == "dd":
                    description = self.child_blocks(children[child_index + 1]).strip()
                    if description:
                        output.append(description + "\n")
                    child_index += 2
                    continue
            else:
                output.append(self.block(child))
            child_index += 1
        return "\n".join(output) + ("\n" if output else "")

    def admonition(self, element) -> str:
        classes = set((element.get("class") or "").split())
        title = "Note"
        title_nodes = element.xpath('./p[contains(concat(" ", normalize-space(@class), " "), " admonition-title ")]')
        if title_nodes:
            title = clean_text(title_nodes[0].text_content()) or title
        body = "".join(
            self.block(child)
            for child in element
            if "admonition-title" not in set((child.get("class") or "").split())
        ).strip()
        if not body:
            return ""
        color = "warning-gold" if classes & {"attention", "caution", "important", "warning"} else "note-blue"
        icon = "!" if color == "warning-gold" else "✎"
        return (
            "#block(width: 100%, inset: (x: 0pt, y: 4pt))[\n"
            "#grid(columns: (0.35in, 1fr), gutter: 0.12in,\n"
            f"  [#text(fill: {color}, size: 18pt)[{icon}]],\n"
            f"  [#text(fill: {color}, weight: \"bold\")[{escape_text(title)}]\\\n{body}]\n"
            ")\n"
            "]\n\n"
        )

    def table(self, element) -> str:
        classes = set((element.get("class") or "").split())
        if "genindextable" in classes:
            return self.index_table(element)

        rows = element.xpath(".//tr")
        if not rows:
            return ""
        column_count = max((len(row.xpath("./th|./td")) for row in rows), default=1)
        entries = []
        has_header = any(cell.tag.lower() == "th" for cell in rows[0].xpath("./th|./td"))
        start_index = 0
        if has_header:
            header_cells = ", ".join(
                f"[{self.cell_text(cell)}]" for cell in rows[0].xpath("./th|./td")
            )
            entries.append(f"table.header({header_cells})")
            start_index = 1
        for row in rows[start_index:]:
            cells = row.xpath("./th|./td")
            entries.extend(f"[{self.cell_text(cell)}]" for cell in cells)
            entries.extend("[]" for _ in range(column_count - len(cells)))
        body = ",\n  ".join(entries)
        return (
            "#figure(table(\n"
            f"  columns: {column_count},\n"
            "  inset: 4pt,\n"
            "  stroke: table-stroke,\n"
            "  fill: (x, y) => if y == 0 { table-head } else { none },\n"
            f"  {body}\n"
            "))\n\n"
        )

    def cell_text(self, cell) -> str:
        return self.child_inline(cell).strip() or escape_text(clean_text(cell.text_content()))

    def index_table(self, element) -> str:
        output = []
        for cell in element.xpath(".//td"):
            content = self.child_blocks(cell).strip()
            if content:
                output.append(content)
        if not output:
            return ""
        content = "\n\n".join(output)
        return (
            "#columns(3, gutter: 12pt)[\n"
            "#text(size: 6.8pt)[\n"
            f"{content}\n"
            "]\n"
            "]\n\n"
        )

    def index_html_path(self) -> pathlib.Path | None:
        # The general index may be emitted beside singlehtml output or under
        # the normal html builder directory, depending on the active Sphinx flow.
        candidates = [
            self.input_html.parent / "genindex.html",
            self.input_html.parent.parent / "html" / "genindex.html",
        ]
        for candidate in candidates:
            if candidate.is_file():
                return candidate
        return None

    def index_body(self) -> str:
        index_path = self.index_html_path()
        if index_path is None:
            return ""
        document = html.parse(str(index_path))
        nodes = document.xpath('//div[@itemprop="articleBody"]') or document.xpath(
            '//div[@role="main"]'
        )
        if not nodes:
            return ""
        self.prepare_pdf_index(document)
        body = self.child_blocks(nodes[0]).strip()
        return f"#pagebreak()\n{body}\n" if body else ""

    def image(self, source: str) -> str:
        source = self.pdf_safe_image_source(source)
        return f"#image({typst_string(source)}{self.image_options(source)})"

    def figure(self, source: str | None) -> str:
        if not source:
            return ""
        source = self.pdf_safe_image_source(source)
        return f"#figure(image({typst_string(source)}{self.image_options(source)}))\n\n"

    def pygments_language(self, element) -> str | None:
        # Sphinx stores the requested lexer on an ancestor of ``pre`` using a
        # class such as ``highlight-python``. Walk upward from the code block so
        # this also works when the actual text is nested in ``code`` or ``span``
        # elements. Generic classes intentionally return ``None`` so Pygments
        # can guess a lexer from the raw text instead of forcing plain text.
        current = element
        while current is not None and hasattr(current, "tag"):
            for class_name in (current.get("class") or "").split():
                if not class_name.startswith("highlight-"):
                    continue
                language = class_name.removeprefix("highlight-")
                if language not in {"default", "none", "text"}:
                    return language
            current = current.getparent()
        return None

    def pygments_token_type(self, element, default=Token.Text):
        # Already-highlighted Sphinx HTML contains nested spans with Pygments
        # CSS classes. If a child span has no recognized class, inherit the
        # parent token type so nested markup does not accidentally drop syntax
        # coloring for its contained text.
        for class_name in (element.get("class") or "").split():
            token_type = PYGMENTS_CLASS_TOKENS.get(class_name)
            if token_type is not None:
                return token_type
        return default

    def highlighted_html_code(self, element, token_type=Token.Text) -> str:
        # Prefer the tokenization Sphinx already generated in HTML. Recursing
        # over text, children, and tails preserves the original code order while
        # mapping each span's Pygments class back to a Typst-styled raw token.
        output = []
        if element.text:
            output.append(typst_code_token(element.text, token_type))
        for child in element:
            child_token_type = self.pygments_token_type(child, token_type)
            output.append(self.highlighted_html_code(child, child_token_type))
            if child.tail:
                output.append(typst_code_token(child.tail, token_type))
        return "".join(output)

    def highlighted_raw_text(self, text: str, language: str | None) -> str:
        try:
            # When Sphinx did not emit token spans, lex the raw text on demand.
            # A known ``highlight-<language>`` class is used directly; otherwise
            # Pygments guesses from the snippet. If both fail, fall back to a
            # plain raw Typst token so PDF generation remains best-effort.
            lexer = get_lexer_by_name(language) if language else guess_lexer(text)
        except ClassNotFound:
            return typst_code_token(text)
        return highlight(text, lexer, TypstPygmentsFormatter())

    def raw_block(self, element) -> str:
        text = element.text_content()
        text = text.rstrip("\n")
        if not text:
            return ""
        # First reuse any Sphinx/Pygments HTML spans present under the ``pre``
        # element. Some generated pages contain plain ``pre`` blocks instead,
        # so empty output from the HTML path triggers raw-text lexing below.
        highlighted_code = self.highlighted_html_code(element).rstrip()
        if not highlighted_code:
            highlighted_code = self.highlighted_raw_text(text, self.pygments_language(element)).rstrip()
        # Keep the outer block styling independent from token coloring: the
        # block controls background, padding, and monospace defaults, while the
        # inner fragments apply per-token fills and font styles.
        return (
            "#block(fill: code-bg, inset: 7pt, radius: 3pt, width: 100%)[\n"
            f"#text(font: \"DejaVu Sans Mono\", size: 9.0pt, fill: code-text)[\n{highlighted_code}\n]\n"
            "]\n\n"
        )

    def convert(self, title: str, author: str) -> str:
        document = html.parse(str(self.input_html))
        self.prepare_index_links(document)
        nodes = document.xpath('//div[@itemprop="articleBody"]') or document.xpath("//body")
        if not nodes:
            raise RuntimeError(f"Could not find article body in {self.input_html}")
        body = self.child_blocks(nodes[0]).strip()
        index_body = self.index_body()
        return f'''// Generated from {self.input_html.name}; do not edit directly.
#let accent = rgb("#1868DB")
#let body-gray = rgb("#555555")
#let rule-gray = rgb("#e6e6e6")
// Cover-page colors keep the first PDF page visually distinct from content pages.
#let cover-bg = rgb("#f6f9ff")
#let cover-ink = rgb("#10213f")
#let cover-muted = rgb("#50617d")
#let cover-orb-blue = rgb("#dbeaff")
#let cover-orb-cyan = rgb("#e8f6ff")
#let cover-pill-stroke = rgb("#d9e5f8")
#let table-stroke = rgb("#dddddd")
#let table-head = rgb("#fafafa")
#let link-blue = rgb("#2d8fd5")
#let code-red = rgb("#d14f73")
#let code-bg = rgb("#f5f3fb")
#let code-text = rgb("#6a6a6a")
#let highlight-yellow = rgb("#f7df73")
#let note-blue = rgb("#5f8ec2")
#let warning-gold = rgb("#b58c38")

#set document(title: {typst_string(title)}, author: {typst_string(author)})
#set page(paper: "a4", margin: 0.72in, numbering: none, fill: cover-bg)
#set text(font: "DejaVu Sans", size: 9.2pt, fill: body-gray)
#set par(justify: true, leading: 0.55em)
#show link: it => text(fill: link-blue, it)
#show heading: it => block(above: 1.5em, below: 1.2em)[
  #text(fill: accent, weight: "regular", size: if it.level == 1 {{ 22pt }} else if it.level == 2 {{ 15pt }} else {{ 11pt }})[#it.body]
]
#show raw.where(block: true): set block(breakable: true)
#show list: set block(spacing: 0.45em)
#show table: set text(size: 8.2pt)
#show figure: set block(above: 0.45em, below: 0.75em)
#show outline.entry: set block(below: 0.18em)

// Splash cover: decorative orbs, prominent title, and a compact metadata pill.
#place(top + right, dx: 0.42in, dy: -0.42in)[#circle(radius: 1.65in, fill: cover-orb-blue)]
#place(bottom + left, dx: -0.32in, dy: 0.28in)[#circle(radius: 1.10in, fill: cover-orb-cyan)]
#v(5.7in)
#line(length: 1.6in, stroke: 2.2pt + accent)
#v(0.28in)
#text(size: 12pt, fill: accent, weight: "bold")[Embedded Linker]
#v(0.10in)
#text(size: 34pt, fill: cover-ink, weight: "bold")[ELD Documentation]
#v(0.18in)
#text(size: 11pt, fill: cover-muted)[A detailed guide to linking, diagnostics, plugins, linker-scripts, etc.]
#v(0.48in)
#grid(columns: (auto, auto), gutter: 0.14in)[
  #rect(fill: accent, stroke: none, radius: 7pt, inset: (x: 11pt, y: 7pt))[#text(size: 8.5pt, fill: white, weight: "bold")[{escape_text(author)}]]
]

#pagebreak()
// Reset regular document pages after the full-page splash cover.
#set page(
  paper: "a4",
  margin: (x: 0.72in, top: 0.82in, bottom: 0.82in),
  numbering: "1",
  fill: white,
  header: context [
    #text(size: 7.5pt, fill: body-gray)[#align(right)[| Page #counter(page).display()]]
    #line(length: 100%, stroke: rule-gray)
  ],
  footer: context [
    #line(length: 100%, stroke: rule-gray)
    #align(right)[#text(size: 7.5pt, fill: body-gray)[{title} | © {author}]]
  ],
)
#counter(page).update(1)

#outline(title: [#text(fill: accent, size: 20pt)[Table of Contents]], indent: auto)
#pagebreak()
#counter(page).update(1)
#heading(level: 1, outlined: false)[{escape_text(title)}]

{body}
{index_body}
'''


def compile_typst(
    typst_file: pathlib.Path,
    pdf_file: pathlib.Path,
    root: pathlib.Path,
    typst_executable: str | None,
) -> None:
    # Match CMake's dependency preference: use the executable when it was found,
    # otherwise rely on the Python package pinned in requirements.txt.
    if typst_executable:
        subprocess.run(
            [
                typst_executable,
                "compile",
                "--root",
                str(root),
                str(typst_file),
                str(pdf_file),
            ],
            check=True,
        )
        return

    import typst

    typst.compile(str(typst_file), output=str(pdf_file), root=str(root))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert Sphinx singlehtml output to Typst and optionally compile a PDF."
    )
    parser.add_argument("input_html", type=pathlib.Path)
    parser.add_argument("output_typst", type=pathlib.Path)
    parser.add_argument("--pdf-output", type=pathlib.Path)
    parser.add_argument("--root", type=pathlib.Path)
    parser.add_argument("--typst-executable")
    parser.add_argument("--title", default="ELD User Guide")
    parser.add_argument("--author", default="Qualcomm Technologies, Inc.")
    args = parser.parse_args()

    converter = TypstConverter(args.input_html)
    args.output_typst.parent.mkdir(parents=True, exist_ok=True)
    args.output_typst.write_text(
        converter.convert(args.title, args.author), encoding="utf-8"
    )

    if args.pdf_output:
        args.pdf_output.parent.mkdir(parents=True, exist_ok=True)
        compile_typst(
            args.output_typst,
            args.pdf_output,
            args.root or args.output_typst.parent,
            args.typst_executable,
        )


if __name__ == "__main__":
    main()
