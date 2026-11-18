'''
The symbol-preemption test checks that the relocation reservation annotations
page contains the expected D2 SVG image, that the browser successfully decodes
it with nonzero intrinsic dimensions, and that it is visible after scrolling
into the viewport. A missing, broken, or hidden diagram fails the test.
'''

# Run this test only for main branch.
TEST_BRANCH = "main"

import re

import pytest
from playwright.sync_api import Page, expect


@pytest.fixture(scope="session")
def docs_page_url(docs_dir):
    docs_page = docs_dir / "DeveloperDocs/Relocation-Reservation-Annotations.html"
    if not docs_page.is_file():
        raise pytest.UsageError(f"Documentation page not found: {docs_page}")

    return docs_page.as_uri()


def test_symbol_preemption_diagram_is_rendered_and_visible(page: Page, docs_page_url):
    page.goto(docs_page_url)

    diagram = page.locator("#visibility-and-preemption .d2-diagram").get_by_role(
        "img",
        name=(
            "Symbol preemption decisions for visibility, binding, resolution "
            "state, link mode, and symbolic options."
        ),
        exact=True,
    )
    expect(diagram).to_have_count(1)
    expect(diagram).to_have_class("d2")
    expect(diagram).to_have_attribute(
        "src", re.compile(r"(?:^|/)_images/d2_SymbolPreemption_[0-9a-f]+\.svg$")
    )
    expect(diagram).to_be_visible()
    expect(diagram).to_have_js_property("complete", True)
    expect(diagram).not_to_have_js_property("naturalWidth", 0)
    expect(diagram).not_to_have_js_property("naturalHeight", 0)
    diagram.scroll_into_view_if_needed()
    expect(diagram).to_be_in_viewport()
