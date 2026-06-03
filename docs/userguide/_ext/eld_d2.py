"""Sphinx support for rendering D2 diagrams."""

from __future__ import annotations

import hashlib
import math
import re
import shutil
import subprocess
from os import path
from pathlib import Path
from subprocess import PIPE, CalledProcessError
from typing import Any

import posixpath
import sphinx
from docutils import nodes
from docutils.nodes import Node
from docutils.parsers.rst import Directive, directives
from sphinx.application import Sphinx
from sphinx.errors import SphinxError
from sphinx.locale import __, _
from sphinx.util.docutils import SphinxDirective
from sphinx.util.i18n import search_image_for_language
from sphinx.util.nodes import set_source_info
from sphinx.util.osutil import ensuredir
from sphinx.util.typing import OptionSpec
from sphinx.writers.html import HTMLTranslator
from sphinx.writers.latex import LaTeXTranslator
from sphinx.writers.manpage import ManualPageTranslator
from sphinx.writers.text import TextTranslator


class D2Error(SphinxError):
    category = "D2 error"


class d2_diagram(nodes.General, nodes.Element):
    pass


def align_spec(argument: Any) -> str:
    return directives.choice(argument, ("left", "center", "right"))


def figure_wrapper(
    directive: Directive, node: d2_diagram, caption: str
) -> nodes.figure:
    figure_node = nodes.figure("", node)
    if "align" in node:
        figure_node["align"] = node.attributes.pop("align")

    inline_nodes, messages = directive.state.inline_text(caption, directive.lineno)
    caption_node = nodes.caption(caption, "", *inline_nodes)
    caption_node.extend(messages)
    set_source_info(directive, caption_node)
    figure_node += caption_node
    return figure_node


class D2Directive(SphinxDirective):
    has_content = True
    required_arguments = 0
    optional_arguments = 1
    final_argument_whitespace = False
    option_spec: OptionSpec = {
        "alt": directives.unchanged,
        "align": align_spec,
        "caption": directives.unchanged,
        "name": directives.unchanged,
        "class": directives.class_option,
    }

    def run(self) -> list[Node]:
        if self.arguments:
            if self.content:
                warning = self.state.document.reporter.warning(
                    __("D2 directive cannot have both content and a filename argument"),
                    line=self.lineno,
                )
                return [warning]

            argument = search_image_for_language(self.arguments[0], self.env)
            relative_filename, filename = self.env.relfn2path(argument)
            self.env.note_dependency(relative_filename)
            try:
                with open(filename, encoding="utf-8") as diagram_file:
                    code = diagram_file.read()
            except OSError:
                warning = self.state.document.reporter.warning(
                    __("External D2 file %r not found or reading it failed")
                    % filename,
                    line=self.lineno,
                )
                return [warning]
        else:
            code = "\n".join(self.content)
            relative_filename = None
            if not code.strip():
                warning = self.state_machine.reporter.warning(
                    __("Ignoring \"d2\" directive without content."),
                    line=self.lineno,
                )
                return [warning]

        diagram_node = d2_diagram()
        diagram_node["code"] = code
        diagram_node["options"] = {"docname": self.env.docname}
        if relative_filename:
            diagram_node["filename"] = relative_filename
        if "alt" in self.options:
            diagram_node["alt"] = self.options["alt"]
        if "align" in self.options:
            diagram_node["align"] = self.options["align"]
        if "class" in self.options:
            diagram_node["classes"] = self.options["class"]

        if "caption" in self.options:
            figure_node = figure_wrapper(self, diagram_node, self.options["caption"])
            self.add_name(figure_node)
            return [figure_node]

        self.add_name(diagram_node)
        return [diagram_node]


def render_d2(
    translator: Any,
    code: str,
    options: dict[str, Any],
    output_format: str,
    filename: str | None = None,
) -> tuple[str, str]:
    d2_bin = translator.builder.config.d2_bin
    d2_args = list(translator.builder.config.d2_args)
    if output_format == "txt" and not any(
        arg == "--ascii-mode" or arg.startswith("--ascii-mode=") for arg in d2_args
    ):
        d2_args.extend(["--ascii-mode", "standard"])
    hash_inputs = [code, str(d2_bin), str(d2_args), output_format]
    if output_format == "png":
        hash_inputs.extend(
            [
                str(translator.builder.config.d2_pdf_chrome_bin),
                str(translator.builder.config.d2_pdf_raster_scale),
            ]
        )
    hash_key = "\n".join(hash_inputs).encode()
    output_name = "d2-%s.%s" % (hashlib.sha1(hash_key).hexdigest(), output_format)
    relative_output = posixpath.join(translator.builder.imgpath, output_name)
    output_path = path.join(
        translator.builder.outdir, translator.builder.imagedir, output_name
    )

    if path.isfile(output_path):
        return relative_output, output_path

    ensuredir(path.dirname(output_path))

    if output_format == "png":
        svg_path = render_d2(
            translator, code, options, "svg", filename
        )[1]
        rasterize_svg_for_pdf(translator, svg_path, output_path)
        return relative_output, output_path

    command = [d2_bin, *d2_args]
    input_bytes = None
    if filename:
        input_path = path.join(translator.builder.srcdir, filename)
        working_dir = path.dirname(input_path)
        command.extend([input_path, output_path])
    else:
        docname = options.get("docname", "index")
        working_dir = path.dirname(path.join(translator.builder.srcdir, docname))
        command.extend(["-", output_path])
        input_bytes = code.encode()

    try:
        result = subprocess.run(
            command,
            input=input_bytes,
            stdout=PIPE,
            stderr=PIPE,
            cwd=working_dir,
            check=True,
        )
    except OSError as error:
        raise D2Error(
            __("d2 command %r cannot be run; check the d2_bin setting") % d2_bin
        ) from error
    except CalledProcessError as error:
        raise D2Error(
            __("d2 exited with error:\n[stderr]\n%r\n[stdout]\n%r")
            % (error.stderr, error.stdout)
        ) from error

    if not path.isfile(output_path):
        raise D2Error(
            __("d2 did not produce an output file:\n[stderr]\n%r\n[stdout]\n%r")
            % (result.stderr, result.stdout)
        )
    return relative_output, output_path


def svg_viewbox_size(svg_path: str) -> tuple[int, int]:
    with open(svg_path, encoding="utf-8") as svg_file:
        svg_start = svg_file.read(4096)

    viewbox_match = re.search(r'viewBox="\s*[-\d.]+\s+[-\d.]+\s+([\d.]+)\s+([\d.]+)', svg_start)
    if viewbox_match:
        return (
            max(1, math.ceil(float(viewbox_match.group(1)))),
            max(1, math.ceil(float(viewbox_match.group(2)))),
        )

    svg_match = re.search(r"<svg\b[^>]*", svg_start)
    if svg_match:
        width_match = re.search(r'\bwidth="([\d.]+)', svg_match.group(0))
        height_match = re.search(r'\bheight="([\d.]+)', svg_match.group(0))
        if width_match and height_match:
            return (
                max(1, math.ceil(float(width_match.group(1)))),
                max(1, math.ceil(float(height_match.group(1)))),
            )

    return (1200, 900)


def find_chrome_bin(config: Any) -> str:
    configured_chrome = config.d2_pdf_chrome_bin
    candidates = [configured_chrome] if configured_chrome else []
    candidates.extend(["google-chrome", "chromium", "chromium-browser", "chrome"])
    for candidate in candidates:
        if not candidate:
            continue
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
        if path.isfile(candidate):
            return candidate

    raise D2Error(
        _(
            "D2 PDF rendering requires Google Chrome or Chromium to rasterize "
            "SVG diagrams. Set d2_pdf_chrome_bin or D2_PDF_CHROME_BIN."
        )
    )


def rasterize_svg_for_pdf(translator: Any, svg_path: str, png_path: str) -> None:
    chrome_bin = find_chrome_bin(translator.builder.config)
    width, height = svg_viewbox_size(svg_path)
    scale = translator.builder.config.d2_pdf_raster_scale
    command = [
        chrome_bin,
        "--headless",
        "--disable-gpu",
        "--hide-scrollbars",
        "--no-sandbox",
        f"--force-device-scale-factor={scale}",
        f"--window-size={width},{height}",
        f"--screenshot={png_path}",
        Path(svg_path).resolve().as_uri(),
    ]

    try:
        result = subprocess.run(command, stdout=PIPE, stderr=PIPE, check=True)
    except OSError as error:
        raise D2Error(
            _("Chrome command %r cannot be run; check d2_pdf_chrome_bin")
            % chrome_bin
        ) from error
    except CalledProcessError as error:
        raise D2Error(
            _("Chrome failed to rasterize D2 SVG:\n[stderr]\n%r\n[stdout]\n%r")
            % (error.stderr, error.stdout)
        ) from error

    if not path.isfile(png_path):
        raise D2Error(
            _("Chrome did not produce a D2 PNG:\n[stderr]\n%r\n[stdout]\n%r")
            % (result.stderr, result.stdout)
        )


def make_d2_image_node(
    translator: Any,
    node: d2_diagram,
    output_format: str,
    absolute_uri: bool = False,
) -> nodes.image:
    relative_output, output_path = render_d2(
        translator, node["code"], node["options"], output_format, node.get("filename")
    )

    image_node = nodes.image(uri=output_path if absolute_uri else relative_output)
    if "alt" in node:
        image_node["alt"] = node["alt"]
    if "align" in node:
        image_node["align"] = node["align"]
    if "classes" in node:
        image_node["classes"] = node["classes"]
    for attribute in ("ids", "names", "dupnames", "backrefs"):
        if attribute in node:
            image_node[attribute] = node[attribute]
    return image_node


def html_visit_d2_diagram(translator: HTMLTranslator, node: d2_diagram) -> None:
    relative_output = render_d2(
        translator, node["code"], node["options"], "svg", node.get("filename")
    )[0]

    alt = node.get("alt", _("D2 diagram"))
    classes = " ".join(["d2-diagram", *node.get("classes", [])])
    if "align" in node:
        translator.body.append(
            '<div align="%s" class="align-%s">' % (node["align"], node["align"])
        )
    translator.body.append('<div class="d2">')
    translator.body.append(
        '<object data="%s" type="image/svg+xml" class="%s">\n'
        % (relative_output, classes)
    )
    translator.body.append('<p class="warning">%s</p>' % translator.encode(alt))
    translator.body.append("</object></div>\n")
    if "align" in node:
        translator.body.append("</div>\n")
    raise nodes.SkipNode


def latex_visit_d2_diagram(translator: LaTeXTranslator, node: d2_diagram) -> None:
    image_node = make_d2_image_node(
        translator, node, translator.builder.config.d2_latex_output_format
    )
    translator.visit_image(image_node)
    raise nodes.SkipNode


def rst2pdf_visit_d2_diagram(translator: Any, node: d2_diagram) -> None:
    if translator.builder.config.d2_pdf_output_format == "txt":
        output_path = render_d2(
            translator,
            node["code"],
            node["options"],
            "txt",
            node.get("filename"),
        )[1]
        with open(output_path, encoding="utf-8") as text_diagram_file:
            text_diagram = text_diagram_file.read()
        literal_node = nodes.literal_block(text_diagram, text_diagram)
        node.parent.replace(node, literal_node)
        raise nodes.SkipNode

    image_node = make_d2_image_node(
        translator,
        node,
        translator.builder.config.d2_pdf_output_format,
        absolute_uri=True,
    )
    node.parent.replace(node, image_node)
    raise nodes.SkipNode


def rst2pdf_depart_d2_diagram(translator: Any, node: d2_diagram) -> None:
    pass


def text_visit_d2_diagram(translator: TextTranslator, node: d2_diagram) -> None:
    translator.add_text(_("[D2 diagram: %s]") % node.get("alt", ""))
    raise nodes.SkipNode


def man_visit_d2_diagram(translator: ManualPageTranslator, node: d2_diagram) -> None:
    translator.body.append(_("[D2 diagram: %s]") % node.get("alt", ""))
    raise nodes.SkipNode


def install_rst2pdf_support() -> None:
    try:
        from rst2pdf.pdfbuilder import PDFTranslator
    except Exception:
        return

    if getattr(PDFTranslator, "_eld_d2_support", False):
        return

    PDFTranslator.visit_d2_diagram = rst2pdf_visit_d2_diagram
    PDFTranslator.depart_d2_diagram = rst2pdf_depart_d2_diagram
    PDFTranslator._eld_d2_support = True


def setup(app: Sphinx) -> dict[str, Any]:
    install_rst2pdf_support()
    app.add_node(
        d2_diagram,
        html=(html_visit_d2_diagram, None),
        latex=(latex_visit_d2_diagram, None),
        text=(text_visit_d2_diagram, None),
        man=(man_visit_d2_diagram, None),
    )
    app.add_directive("d2", D2Directive)
    app.add_config_value("d2_bin", "d2", "html")
    app.add_config_value("d2_args", [], "html")
    app.add_config_value("d2_pdf_output_format", "png", "html")
    app.add_config_value("d2_pdf_chrome_bin", "", "html")
    app.add_config_value("d2_pdf_raster_scale", 2, "html")
    app.add_config_value("d2_latex_output_format", "pdf", "html")
    app.add_css_file("d2.css")
    return {"version": sphinx.__display_version__, "parallel_read_safe": True}
