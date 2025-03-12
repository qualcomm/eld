"""
Sphinxextension for QC Latex Builder.
Outputs a LaTeX file that is conformant with the
format that the Qualcomm TechPubs team wants.
"""

### This is still a work in progress. There may be a number of
### of docutils and sphinx nodes not handled by the visitors
### in QCLaTeXTranslator. Over time, we can keep extending the
### capabilities of QCLaTeXTranslator

import os
import logging
from sphinx.builders.latex import LaTeXBuilder
from sphinx import package_dir
from sphinx.writers.latex import LaTeXTranslator
from sphinx.util.template import LaTeXRenderer
from sphinx.util import logging
from docutils import nodes
import docutils
import re
from sphinx.locale import admonitionlabels

logger = logging.getLogger(__name__)
# Some useful links to learn about the important classes in docutils.
# Docutils document tree : https://docutils.sourceforge.io/docs/ref/doctree.html
# https://docutils.sourceforge.io/docs/user/latex.html
# http://epydoc.sourceforge.net/docutils/public/docutils.nodes.Node-class.html
# http://epydoc.sourceforge.net/docutils/public/docutils.nodes.Element-class.html
# To see the super class implementations, see
# /usr/lib/python2.7/dist-packages/sphinx/writers/latex.py

# We look these arrays up by indexing into them using self.sectionlevel.
# There are no parts, so a self.sectionlevel of 0 is also a chapter

class QCTable(object):
    """A Table of data customized for QCLatex"""

    def __init__(self, node):
    # type: (nodes.table) -> None
        self.align = node.get('align')
        self.colwidth = []      # type: List[int]
        self.colwidth_float = [] # type: List[float]
        self.rows = []          # type: List[QCTableCell]
        self.cols = 0
        self.rols = 0
        self.num_cols = 0
        self.num_rows = 0
        self.curr_row = -1
        self.curr_col = -1
        self.in_header = False
        self.body = []
        self.header = []        # type: List[unicode]

    def add_colwidth(self, width):
        self.colwidth.append(int(width))

    def is_first_col(self, col_no):
        return col_no == 0

    def is_last_col(self, col_no):
        return col_no == (self.num_cols - 1)

    def get_col_spec(self, width):
        return "|>{\\raggedright}p{%.1fin}" % width
    def get_col_specs(self):
        assert len(self.colwidth) == self.num_cols, "len(colwidth:%d) != num_cols:%d\n" % (len(self.colwidth), self.num_cols)
        assert len(self.colwidth) <= MAX_COLS_IN_TABLE, "Cannot have more than %d Columns in table" % MAX_COLS_IN_TABLE
        # Techpubs have converted our erstwhile PDF into tex and given that to me
        # Looking at that, I see we have about 6.25 in to divide between columns
        body = "{"
        for cw in self.colwidth:
            w = float()
            w = (float(cw)/100.0) * TOTAL_WIDTH_OF_TABLE
            body += self.get_col_spec(w)
            self.colwidth_float.append(w)

        body += "|}"
        return body
# class QCTableCell(object):

#     def __init__(self, node):
#     # type: (nodes.entry) -> None

LATEX_SEC_NAMES_PREFIX = ["ch", "ch", "sec", "sec", "sec"]
LATEX_SEC_NAMES = [ "Chapter", "Chapter", "Section", "Section", "Section"]
MAX_COLS_IN_TABLE = int(4)
TOTAL_WIDTH_OF_TABLE = float(6.25)
class QCLaTeXTranslator(LaTeXTranslator):

    def __init__(self, document, builder):
        print(docutils.__version__)
        print(docutils.__file__)
        super(QCLaTeXTranslator, self).__init__(document, builder)
        # If we do not reset then we get \\sphinxtableofcontents
        self.elements["tableofcontents"] = ""
        # This could well have been self.in_code. However, not all code is equal.
        # There are latex escape rules implemented by sphinx (sphinx.util.texscapes)
        # As per these rules all underscores (_) in a text node get escaped. So a _
        # becomes "\_". We do not want this behaviour in code snippets.
        # So, in visit_text, we check if self.in_code is 1. If yes, we take the
        # text as it is. Else, we let sphinx.util.texscapes work on it by calling the
        # visitor in the base class.
        self.in_code = 0
        self.refs = {}
        self.skip_text_in_ref = False
        self.in_table = 0
        # We do not put release notes in the pdf because the pdf is relevant for the latest
        # release.
        self.skip_sections = {"indices-and-tables", "release-notes-for-version-release"}
        logger.info("In the constructor of QCLaTeXTranslator")
        logger.info("self.top_sectionlevel = %d" % self.top_sectionlevel)

    def astext(self):
        # type: () -> unicode
        # print "printing body"
        # print self.body
        # print "don printing body"
        self.fix_up_refs()
        self.elements.update({
            'body': ''.join(self.body),
            'indices': self.generate_indices()
        })
        return self.render('ELD_latex.tex_t', self.elements)

    def escape_text(self, text):
        return text.replace('_', "\\_")

    # Functions for handling references.
    # If a line has "FIXUP_SEC_TYPE~\ref{FIXUP:label_name}, this function
    # find the right prefix for label_name and replaces
    # it in line. So, for example, if label_name is a reference
    # to a section, line will now contain \ref{sec:label_name}
    def do_fix_up(self, line):
        expression = re.compile('.*ref{FIXUP:(.*)}')
#        print line
        match = expression.match(line)
        if match:
            name = match.group(1)
            logger.info("In fixup. link name is %s" % name)
            # Returns "ch:name" or "sec:name" depending on if name is a chapter or a section.
            link_name = self.get_link_name(name)

            # Returns Section or Chapter for use in Section~\ref{} or Chapter~\ref{}
            sec_type = self.get_sec_type_for_link(name)
            return line.replace("FIXUP:"+name, link_name).replace("FIXUP_SEC_TYPE", sec_type)
        else:
            return line

    # fix up all references in self.body with their appropriate
    # prefixes. So, all instances of \ref{FIXUP:label_name} will
    # become \ref{ch:label_name} if label_name is a reference to
    # a chapter
    def fix_up_refs(self):
        print("BODY->")
        print(self.body)
        for index, line in enumerate(self.body):
            fixed_up_line = self.do_fix_up(line)
            if line != fixed_up_line:
                logger.info("Replacing %s with %s" %(line, fixed_up_line))
                self.body[index] = fixed_up_line

    def hypertarget(self, id, withdoc=True, anchor=True):
        # type: (unicode, bool, bool) -> unicode
        self.record_target(id, self.sectionlevel)
        link = self.idescape(self.get_link_name(id))
        logger.info("Add label as %s" % link)
        return (anchor and '\\phantomsection' or '') + \
            '\\label{%s}\n' % link

    def idescape(self, id):
        # type: (unicode) -> unicode
        logger.info("id in idescape is %s" % id)
        return id

    def render(self, template_name, variables):
        # type: (unicode, Dict) -> unicode
        template_path = os.path.join(self.builder.app.config.rst2qclatex_templates, template_name)
        if os.path.exists(template_path):
            return LaTeXRenderer().render(template_path, variables)
        else:
            return LaTeXRenderer().render(template_name, variables)

    def record_target(self, name, section_level):
        if name in self.refs:
            logger.info("label %s already present!" % name)
        logger.info("Adding target %s at level %d" % (name, section_level))
        self.refs[name] = section_level

    # Given a link name and a section_level, return the appropriate
    # link name with prefix. Do not call this directly. Use
    # get_link_name instead.
    def get_formatted_link(self, name, section_level):
        try:
            full_link = LATEX_SEC_NAMES_PREFIX[section_level] + ":" + name
        except KeyError:
            logger.warning("Bad section_level %d for %s" % (section_level, name))
            return "unknwon:%s" % name
        return full_link

    def get_sec_level_for_link(self, name):
        try:
            section_level = self.refs[name]
        except KeyError:
            return -1
        return section_level

    # Given the name of a reference, this returns the full link name with prefix:
    # So, if lable (link) is "execution-modes" and it points to a section, this
    # function will return "sec:execution-modes"
    def get_link_name(self, name, section_level=None):
        if section_level is None:
            section_level = self.get_sec_level_for_link(name)

        if section_level == -1:
            logger.warning("label %s not found" % name)
            return "FIXUP:" + name
        return self.get_formatted_link(name, section_level)

    def get_sec_type_for_link(self, name):
        section_level = self.get_sec_level_for_link(name)

        if section_level == -1:
            return "FIXUP_SEC_TYPE"
        else:
            return LATEX_SEC_NAMES[section_level]

    def visit_document(self, node):
        # type: (nodes.Node) -> None
        # We come here only once when index.rst is opened.
        # Through the toctree directive in index.rst all the other rst files are opened
        # and entered through visit_start_of_file. That means, these rst files come
        # into the AST at start_of_file nodes.
        logger.info("visit_document")
        print(node)
        self.footnotestack.append(self.collect_footnotes(node))
        self.curfilestack.append(node.get('docname', ''))
        self.sectionlevel = self.top_sectionlevel - 1
        if self.first_document == 1:
            # the first document is all the regular content ...
            self.first_document = 0
            # This is index.rst which will just turn into a chapter called
            # 'Overview'.
            self.body.append('\chapter{Overview}\n')
        elif self.first_document == 0:
            # ... and all others are the appendices
            self.body.append('\n\n\\appendix\n')
            self.first_document = -1
        if 'docname' in node:
            self.body.append(self.hypertarget(':doc'))
        # "- 1" because the level is increased before the title is visited


    def depart_document(self, node):
        logger.info("depart_document\n")
        # Techpubs requirement:
        # A new line after a chapter heading.
        self.body.append("\n")
        super(QCLaTeXTranslator, self).depart_document(node)

    def visit_strong(self, node):
        # type: (nodes.Node) -> None
        logger.info("visit_strong")
        self.body.append(r'\textbf{')

    def depart_strong(self, node):
        # type: (nodes.Node) -> None
        logger.info("depart_strong")
        self.body.append('}')

    def visit_literal_block(self, node):
        # type: (nodes.Node) -> None
        logger.info("visit_literal_block")
        print(node)
        lang = node.get('language', "")
        if not lang:
            self.body.append("\n\\small\n")
            self.body.append("\\begin{verbatim}\n")
            self.context.append("\n\\end{verbatim}\n\\normalsize\n")
        elif lang == "c++" or lang == "C++":
            self.body.append("\n\\small\n")
            self.body.append("\\begin{lstlisting}[language=halide]\n")
            self.context.append("\n\\end{lstlisting}\n\\normalsize\n")
            self.in_code = 1
        elif lang == "shell" or lang == "bash":
            self.body.append("\n\\small\n")
            self.body.append("\\begin{lstlisting}\n")
            self.context.append("\n\\end{lstlisting}\n\\normalsize\n")
            self.in_code = 1
        elif lang == "python" or lang == "Python":
            self.body.append("\n\\small\n")
            self.body.append("\\begin{lstlisting}[language=Python]\n")
            self.context.append("\n\\end{lstlisting}\n\\normalsize\n")
            self.in_code = 1
        # Specified language not handled.
        else:
            self.body.append("\n\\small\n")
            self.body.append("\\begin{verbatim}\n")
            self.context.append("\n\\end{verbatim}\n\\normalsize\n")

    def depart_literal_block(self, node):
        logger.info("depart_literal_block")
        self.body.append(self.context.pop())
        self.in_code = 0

    def _make_custom_visit_admonition(name):
        # type: (unicode) -> Callable[QCLaTeXTranslator, nodes.Node], None]
        # We let all admonitions be regular paragraphs.
        def custom_visit_admonition(self, node):
            # type: (nodes.Node) -> None
            logger.info("visit_%s" % admonitionlabels[name] )
        return custom_visit_admonition

    def custom_depart_admonition(self, node):
        # type: (nodes.Node) -> None
        logger.info("depart_admonition")
        self.body.append('\n')


    visit_attention = _make_custom_visit_admonition('attention')
    depart_attention = custom_depart_admonition
    visit_caution = _make_custom_visit_admonition('caution')
    depart_caution = custom_depart_admonition
    visit_danger = _make_custom_visit_admonition('danger')
    depart_danger = custom_depart_admonition
    visit_error = _make_custom_visit_admonition('error')
    depart_error = custom_depart_admonition
    visit_hint = _make_custom_visit_admonition('hint')
    depart_hint = custom_depart_admonition
    visit_important = _make_custom_visit_admonition('important')
    depart_important = custom_depart_admonition
    visit_tip = _make_custom_visit_admonition('tip')
    depart_tip = custom_depart_admonition
    visit_warning = _make_custom_visit_admonition('warning')
    depart_warning = custom_depart_admonition

    def visit_note(self, node):
        # type: (nodes.Node) -> None
        logger.info("visit_note")
        self.body.append("\n\\hangindent1.085cm {\\footnotesize\\bf NOTE: } ")

    def depart_note(self, node):
        # type: (nodes.Node) -> None
        logger.info("depart_note")
        self.body.append("\n")

    def visit_reference(self, node):
        # References are of 4 types
        # 1. Email
        # 2. Web URL
        # 3. Cross reference to a location in some other doc. These
        #    are of the form 'refuri'="%doc_name:#location"
        # 4. Reference to a label in the same document. These are of
        #    the form 'refuri'="#location"
        # 5. Reference to an entire document. These are of the form
        #    'refuri'="%doc_name"
        # We will handle 3, 4 and 5 similarly and ensure that the names
        # of labels in our rst files are unique.
        # Further, for every reference we need to know if that reference
        # is a reference to a
        # 1. Chapter (self.sectionlevel = 1)
        # 2. Section or subsection (self.sectionlevel = 2 or 3)
        # We then generate the following
        # "Chapter~\ref{ch:label_name}" - for chapters
        # "Section~\ref{sec:label_name}" - for section and subsection

        # Links in the document are done in two passes. A link is basically
        # a pair of targets (\label) and references (\ref).
        # Targets are converted into labels in visit_target and references
        # are generated in visit_reference.
        # If we visit a reference before we have seen the target (forward
        # reference), then we do not know the type of the target (chapter, section).
        # So, we insert a reference of the form \ref{FIXUP:link_name}
        # In the second pass, just before rendering, we walk self.body and fix
        # up all the references (because by then we should have seen all targets).

        # type: (nodes.Node) -> None
        logger.info("visit_reference")
        uri = node.get('refuri', '')

        if not uri and node.get('refid'):
            uri = '%' + self.curfilestack[-1] + '#' + node['refid']
            logger.info("refid: %s" % node['refid'])

        if uri:
            logger.info("uri = %s" % uri)
            id = ""
            if uri.startswith('%'):
                # references to documents or labels inside documents
                # Case 3 & 5 above.
                hashindex = uri.find('#')
                if hashindex == -1:
                    # reference to the document
                    id = uri[1:]
                else:
                    # reference to a label
                    # Case 4 above.
                    id = uri[hashindex+1:]

                    logger.info("id is %s" % id)
                    link_name = self.get_link_name(id)
                    logger.info("link_name = %s" % link_name)
                    self.body.append("%s~\\ref{%s}" % (self.get_sec_type_for_link(id),
                                               link_name))
#                    self.body.append("%s~\\ref{%s}" % (LATEX_SEC_NAMES[self.get_sec_level_for_link(id)],
 #                                              link_name))
                    self.skip_text_in_ref = True
            elif "@" in uri:
                # Case 1
                self.body.append("{\\color{blue}{\\href{mailto:%s}{%s}}}" %(self.escape_text(uri), self.escape_text(node[0])))
                self.skip_text_in_ref = True
            elif "http" in uri:
                # Case 2
                self.body.append("{\\color{blue}{\\href{%s}{%s}}}" %(self.escape_text(uri), self.escape_text(node[0])))
                self.skip_text_in_ref = True
        else:
            logger.info("Going to the parent reference handler")
            super(QCLaTeXTranslator, self).visit_reference(node)

    def visit_literal(self, node):
        # type: (nodes.Node) -> None
        logger.info("visit_literal")
        self.no_contractions += 1
        if self.in_term > 0:
            pass
        elif self.in_title:
            # We'll assume titles do not have $ and {. These mess with delimiters
            # See more info below.
            self.body.append('{\\small{\\lstinline{')
        else:
            self.in_code = 1
            # For lstinline we use ! as the delimiter because
            # our code can contain $,{ as well.
            # See:
            # https://tex.stackexchange.com/questions/296377/lstinline-source-code
            if self.in_table == 1:
                self.body.append('{\\footnotesize{\\lstinline!')
            else:
                self.body.append('{\\small{\\lstinline!')

    def depart_literal(self, node):
        # type: (nodes.Node) -> None
        logger.info("depart_literal\n")
        if self.in_term > 0:
            pass
        elif self.in_title:
            self.body.append('}}}')
        else:
            # This means we are in inline code.
            # Whether in a table or not, we need
            # to close lstinline and then close footnotesize
            # if in a table or \small if not.
            self.body.append('!}}')
            self.in_code = 0


    def visit_section(self, node):
        #type: (nodes.Node) -> Node
        logger.info("visit_section")
        # Techpubs requirement: Two blank lines before and after a section.
        self.body.append("\n\n")
        if 'ids' in node:
            ids = node['ids']
            for id in ids:
                if id in self.skip_sections:
                    logger.info("Skipping section with id = %s" % id)
                    raise nodes.SkipNode
            else:
                super(QCLaTeXTranslator, self).visit_section(node)
        else:
            super(QCLaTeXTranslator, self).visit_section(node)


    def depart_section(self, node):
        #type: (nodes.Node) -> Node
        logger.info("depart_section\n")
        super(QCLaTeXTranslator, self).depart_section(node)
        # Techpubs requirement: Two blank lines before and after a section.
        self.body.append("\n\n")

    def visit_Text(self, node):
        logger.info("visit_Text\n")
        # In the html code, we print an overlying text on our references.
        # However, in latex, we only generate section numbers.
        if self.skip_text_in_ref != True:
            if self.in_code == 1:
                self.body.append(node.astext())
            else:
                super(QCLaTeXTranslator, self).visit_Text(node)

    def visit_bullet_list(self, node):
        logger.info("visit_bullet_list\n")
        # Techpubs requuirement:
        # Techpubs wants a blank line between a paragraph and a list.
        # Its hard to keep track of sibling context. So, simply just emit
        # a line unconditionally
        self.body.append("\n")
        super(QCLaTeXTranslator, self).visit_bullet_list(node)

    def depart_bullet_list(self, node):
        logger.info("depart_bullet_list\n")
        # Techpubs requirement:
        # Techpubs wants a blank line after every list.
        super(QCLaTeXTranslator, self).depart_bullet_list(node)
        self.body.append("\n")

    def visit_hlist(self, node):
        logger.info("visit_hlist\n")
        # Techpubs wants a blank line between a paragraph and a list.
        # Its hard to keep track of sibling context. So, simply just emit
        # a line unconditionally
        super.body.append("\n")
        super(QCLaTeXTranslator, self).visit_hlist(node)

    def depart_hlist(self, node):
        logger.info("depart_hlist\n")
        # Techpubs requirement:
        # Techpubs wants a blank line after every list.
        super(QCLaTeXTranslator, self).depart_hlist(node)
        super.body.append("\n")

    def visit_list_item(self, node):
        # Techpubs requirement
        # The default latex builder (super) adds
        # '\item {}' for a list item. Techpubs does not
        # want the curly braces.
        logger.info("visit_list_item\n")
        self.body.append("\item")

    def visit_inline(self, node):
        # visit_inline in the base class adds sphinx
        # styles which we do not care for.
        logger.info("visit_inline\n")
        pass

    def depart_inline(self, node):
        logger.info("depart_inline\n")
        pass

    # Indirections for debugging
    def visit_paragraph(self, node):
        logger.info("visit_paragraph\n")
        # Techpubs requirement: A blank line before and after every paragraph.
        # However, adding this without context can mean we could end up with more
        # than just one line before and after paragraphs.
        if self.in_table != 1:
            if isinstance(node.parent, nodes.note):
                # If the parent node is a note, do not insert a blank line
                logger.info("Not adding a blank line in paragraph inside a note\n");
                pass
            else:
                self.body.append("\n")
                super(QCLaTeXTranslator, self).visit_paragraph(node)


    def depart_paragraph(self, node):
        logger.info("depart_paragraph\n")

        if self.in_table != 1:
            # Techpubs requirement is to add a blank line before and after a paragraph
            # depart_paragraph in super does that anyway.
            super(QCLaTeXTranslator, self).depart_paragraph(node)


    def visit_start_of_file(self, node):
        logger.info("visit_start_of_file\n")
        super(QCLaTeXTranslator, self).visit_start_of_file(node)
    def visit_highlightlang(self, node):
        logger.info("visit_highlightlang\n")
        super(QCLaTeXTranslator, self).visit_highlightlang(node)
    def visit_problematic(self, node):
        logger.info("visit_problematic\n")
        super(QCLaTeXTranslator, self).visit_problematic(node)
    def visit_topic(self, node):
        logger.info("visit_topic\n")
        raise nodes.SkipNode

    def visit_glossary(self, node):
        logger.info("visit_glossary\n")
        super(QCLaTeXTranslator, self).visit_glossary(node)
    def visit_productionlist(self, node):
        logger.info("visit_productionlist\n")
        super(QCLaTeXTranslator, self).visit_productionlist(node)
    def visit_production(self, node):
        logger.info("visit_production\n")
        super(QCLaTeXTranslator, self).visit_production(node)
    def visit_transition(self, node):
        logger.info("visit_transition\n")
        super(QCLaTeXTranslator, self).visit_transition(node)
    def visit_title(self, node):
        logger.info("visit_title\n")
        parent = node.parent
        if isinstance(parent, nodes.table):
            # Title of a table.
            # FIXME: For now no printing of title in table.
            # FIXME:  Handle titles for tables
            raise nodes.SkipNode
        else:
           super(QCLaTeXTranslator, self).visit_title(node)
    def visit_subtitle(self, node):
        logger.info("visit_subtitle\n")
        super(QCLaTeXTranslator, self).visit_subtitle(node)
    def visit_desc(self, node):
        logger.info("visit_desc\n")
        super(QCLaTeXTranslator, self).visit_desc(node)
    def visit_desc_signature(self, node):
        logger.info("visit_desc_signature\n")
        super(QCLaTeXTranslator, self).visit_desc_signature(node)
    def visit_desc_signature_line(self, node):
        logger.info("visit_desc_signature_line\n")
        super(QCLaTeXTranslator, self).visit_desc_signature_line(node)
    def visit_desc_addname(self, node):
        logger.info("visit_desc_addname\n")
        super(QCLaTeXTranslator, self).visit_desc_addname(node)
    def visit_desc_type(self, node):
        logger.info("visit_desc_type\n")
        super(QCLaTeXTranslator, self).visit_desc_type(node)
    def visit_desc_returns(self, node):
        logger.info("visit_desc_returns\n")
        super(QCLaTeXTranslator, self).visit_desc_returns(node)
    def visit_desc_name(self, node):
        logger.info("visit_desc_name\n")
        super(QCLaTeXTranslator, self).visit_desc_name(node)
    def visit_desc_parameterlist(self, node):
        logger.info("visit_desc_parameterlist\n")
        super(QCLaTeXTranslator, self).visit_desc_parameterlist(node)
    def visit_desc_parameter(self, node):
        logger.info("visit_desc_parameter\n")
        super(QCLaTeXTranslator, self).visit_desc_parameter(node)
    def visit_desc_optional(self, node):
        logger.info("visit_desc_optional\n")
        super(QCLaTeXTranslator, self).visit_desc_optional(node)
    def visit_desc_annotation(self, node):
        logger.info("visit_desc_annotation\n")
        super(QCLaTeXTranslator, self).visit_desc_annotation(node)
    def visit_desc_content(self, node):
        logger.info("visit_desc_content\n")
        super(QCLaTeXTranslator, self).visit_desc_content(node)
    def visit_seealso(self, node):
        logger.info("visit_seealso\n")
        super(QCLaTeXTranslator, self).visit_seealso(node)
    def visit_rubric(self, node):
        logger.info("visit_rubric\n")
        super(QCLaTeXTranslator, self).visit_rubric(node)
    def visit_footnote(self, node):
        logger.info("visit_footnote\n")
        super(QCLaTeXTranslator, self).visit_footnote(node)
    def visit_collected_footnote(self, node):
        logger.info("visit_collected_footnote\n")
        super(QCLaTeXTranslator, self).visit_collected_footnote(node)
    def visit_label(self, node):
        logger.info("visit_label\n")
        super(QCLaTeXTranslator, self).visit_label(node)
    def visit_tabular_col_spec(self, node):
        logger.info("visit_tabular_col_spec\n")
        super(QCLaTeXTranslator, self).visit_tabular_col_spec(node)

    # Doc structure of a table is
    # <table>
    #   <tgroup cols="3">
    #      <colspec>
    #      <colspec>
    #      <colspec>
    #      <thead>
    #         <row>
    #           <entry> <entry> <entry>
    #         </row>
    #      </thead>
    #      <tbody>
    #         <row>
    #           <entry> <entry> <entry>
    #         </row>
    #      </tbody>
    #   </tgroup>
    # </table>

    def setup_default_table_start(self):
        # \renewcommand{\thetable}{\thechapter-\arabic{table}}
        # \setlength\LTleft\parindent %Left aligns table.
        # \setlength\LTright\fill     %Left aligns table.
        self.body.append("\n\\renewcommand{\\thetable}{\\thechapter-\\arabic{table}}\n")
        self.body.append("\\setlength\\LTleft\\parindent\n")
        self.body.append("\\setlength\\LTright\\fill\n")

    def visit_table(self, node):
        logger.info("visit_table\n")
        classes = node.get('classes', [])
        for c in classes:
            print(c)
        self.setup_default_table_start()
        self.table = QCTable(node)
        self.in_table = 1
        self.body.append("\\begin{longtable}[htb]")
        # We now need to gather information about the columns
        # But, we won't know that till we see all the colspecs.
        # So, we will push what we have of the body on the body
        # stack and work on a new scratch body.
        # Visiting all colspecs is finished by the time
        # we get to visit_thead. At this point, we can pop
        # the old body back and add the scratch body to it.
        # But, wait! we only visit tgroup and colspec from here
        # and neither of those change self.body. So, we can
        # get away without this whole push-pop business for now.

    def visit_colspec(self, node):
        logger.info("visit_colspec\n")
        if 'colwidth' in node:
            self.table.add_colwidth(node['colwidth'])
        self.table.num_cols += 1
#        super(QCLaTeXTranslator, self).visit_colspec(node)
    def visit_tgroup(self, node):
        # Even the superclass does nothing. Do nothing!
        logger.info("visit_tgroup\n")

    def visit_thead(self, node):
        logger.info("visit_thead\n")
        self.table.in_header = True
        # first add the colspecs
        self.body.append(self.table.get_col_specs()+ "\n")
        self.pushbody(self.table.header)
#        super(QCLaTeXTranslator, self).visit_thead(node)

    def visit_tbody(self, node):
        logger.info("visit_tbody\n")
#        super(QCLaTeXTranslator, self).visit_tbody(node)
    def visit_row(self, node):
        logger.info("visit_row\n")
        self.table.num_rows += 1
        self.table.curr_row += 1
        self.body.append("\\hline\n")
    def get_multicolumn_one(self, column_no, num_columns):
        # Left Edge
        if column_no == 0:
#            return "\\multicolumn{1}{|c|}{\\sf\\small"
            return "\\multicolumn{1}{|>{\Cwrap}m{%.1fin}|}{\\sf\\small" % self.table.colwidth_float[column_no]
        else:
            # Middle or right edge
            return "\\multicolumn{1}{>{\Cwrap}m{%.1fin}|}{\\sf\\small" % self.table.colwidth_float[column_no]



    def visit_entry(self, node):
        logger.info("visit_entry\n")
        self.table.curr_col += 1
        print("node is \n")
        print(node)
        print(len(node.children))
        # FIXME: assert for multirow and mulitcol or is it morerow
        if not self.table.is_first_col(self.table.curr_col):
            self.body.append (" & ")
        if self.table.in_header:
            # We are in the header. Use multicolumn{1}
            self.body.append(self.get_multicolumn_one(self.table.curr_col,
                                                      self.table.num_cols)
                             + "\\textbf{")
            # One for multicolum and the other to close \textbf
            self.context.append("}}")

#        super(QCLaTeXTranslator, self).visit_entry(node)
    def visit_acks(self, node):
        logger.info("visit_acks\n")
        super(QCLaTeXTranslator, self).visit_acks(node)
    def visit_enumerated_list(self, node):
        logger.info("visit_enumerated_list\n")
        super(QCLaTeXTranslator, self).visit_enumerated_list(node)
    def visit_definition_list(self, node):
        logger.info("visit_definition_list\n")
        # Techpubs requirement for readability:
        # Put blank linke before \begin{description}
        self.body.append("\n")
        super(QCLaTeXTranslator, self).visit_definition_list(node)
    def visit_definition_list_item(self, node):
        logger.info("visit_definition_list_item\n")
        super(QCLaTeXTranslator, self).visit_definition_list_item(node)
    def visit_term(self, node):
        logger.info("visit_term\n")
        super(QCLaTeXTranslator, self).visit_term(node)
    def visit_classifier(self, node):
        logger.info("visit_classifier\n")
        super(QCLaTeXTranslator, self).visit_classifier(node)
    def visit_definition(self, node):
        logger.info("visit_definition\n")
        super(QCLaTeXTranslator, self).visit_definition(node)
    def visit_field_list(self, node):
        logger.info("visit_field_list\n")
        super(QCLaTeXTranslator, self).visit_field_list(node)
    def visit_field(self, node):
        logger.info("visit_field\n")
        super(QCLaTeXTranslator, self).visit_field(node)
    def visit_centered(self, node):
        logger.info("visit_centered\n")
        super(QCLaTeXTranslator, self).visit_centered(node)
    def visit_hlistcol(self, node):
        logger.info("visit_hlistcol\n")
        super(QCLaTeXTranslator, self).visit_hlistcol(node)
    def visit_image(self, node):
        logger.info("visit_image\n")
        super(QCLaTeXTranslator, self).visit_image(node)
    def visit_figure(self, node):
        logger.info("visit_figure\n")
        super(QCLaTeXTranslator, self).visit_figure(node)
    def visit_caption(self, node):
        logger.info("visit_caption\n")
        # Skip captions for now because they add commands defined in sphinx.sty
#        super(QCLaTeXTranslator, self).visit_caption(node)
    def visit_legend(self, node):
        logger.info("visit_legend\n")
        super(QCLaTeXTranslator, self).visit_legend(node)
    # def visit_admonition(self, node):
    #     logger.info("visit_admonition\n")
    #     super(QCLaTeXTranslator, self).visit_admonition(node)
    # def visit_admonition(self, node):
    #     logger.info("visit_admonition\n")
    #     super(QCLaTeXTranslator, self).visit_admonition(node)
    def visit_versionmodified(self, node):
        logger.info("visit_versionmodified\n")
        super(QCLaTeXTranslator, self).visit_versionmodified(node)
    def visit_target(self, node):
        logger.info("visit_target\n")
        super(QCLaTeXTranslator, self).visit_target(node)
    def visit_attribution(self, node):
        logger.info("visit_attribution\n")
        super(QCLaTeXTranslator, self).visit_attribution(node)
    def visit_raw(self, node):
        logger.info("visit_raw\n")
        super(QCLaTeXTranslator, self).visit_raw(node)
    def visit_number_reference(self, node):
        logger.info("visit_number_reference\n")
        super(QCLaTeXTranslator, self).visit_number_reference(node)
    def visit_download_reference(self, node):
        logger.info("visit_download_reference\n")
        super(QCLaTeXTranslator, self).visit_download_reference(node)
    def visit_pending_xref(self, node):
        logger.info("visit_pending_xref\n")
        super(QCLaTeXTranslator, self).visit_pending_xref(node)
    def visit_emphasis(self, node):
        logger.info("visit_emphasis\n")
        self.body.append(r'\textit{')

    def visit_literal_emphasis(self, node):
        logger.info("visit_literal_emphasis\n")
        super(QCLaTeXTranslator, self).visit_literal_emphasis(node)
    # def visit_strong(self, node):
    #     logger.info("visit_strong\n")
    #     super(QCLaTeXTranslator, self).visit_strong(node)
    def visit_literal_strong(self, node):
        logger.info("visit_literal_strong\n")
        super(QCLaTeXTranslator, self).visit_literal_strong(node)
    def visit_abbreviation(self, node):
        logger.info("visit_abbreviation\n")
        super(QCLaTeXTranslator, self).visit_abbreviation(node)
    def visit_manpage(self, node):
        logger.info("visit_manpage\n")
        super(QCLaTeXTranslator, self).visit_manpage(node)
    def visit_title_reference(self, node):
        logger.info("visit_title_reference\n")
        super(QCLaTeXTranslator, self).visit_title_reference(node)
    def visit_citation(self, node):
        logger.info("visit_citation\n")
        super(QCLaTeXTranslator, self).visit_citation(node)
    def visit_citation_reference(self, node):
        logger.info("visit_citation_reference\n")
        super(QCLaTeXTranslator, self).visit_citation_reference(node)
    # def visit_literal(self, node):
    #     logger.info("visit_literal\n")
    #     super(QCLaTeXTranslator, self).visit_literal(node)
    def visit_footnote_reference(self, node):
        logger.info("visit_footnote_reference\n")
        super(QCLaTeXTranslator, self).visit_footnote_reference(node)
    def visit_line(self, node):
        logger.info("visit_line\n")
        super(QCLaTeXTranslator, self).visit_line(node)
    def visit_line_block(self, node):
        logger.info("visit_line_block\n")
        super(QCLaTeXTranslator, self).visit_line_block(node)
    def visit_block_quote(self, node):
        logger.info("visit_block_quote\n")
        super(QCLaTeXTranslator, self).visit_block_quote(node)
    def visit_option(self, node):
        logger.info("visit_option\n")
        super(QCLaTeXTranslator, self).visit_option(node)
    def visit_option_argument(self, node):
        logger.info("visit_option_argument\n")
        super(QCLaTeXTranslator, self).visit_option_argument(node)
    def visit_option_group(self, node):
        logger.info("visit_option_group\n")
        super(QCLaTeXTranslator, self).visit_option_group(node)
    def visit_option_list(self, node):
        logger.info("visit_option_list\n")
        super(QCLaTeXTranslator, self).visit_option_list(node)
    def visit_option_list_item(self, node):
        logger.info("visit_option_list_item\n")
        super(QCLaTeXTranslator, self).visit_option_list_item(node)
    def visit_option_string(self, node):
        logger.info("visit_option_string\n")
        super(QCLaTeXTranslator, self).visit_option_string(node)
    def visit_description(self, node):
        logger.info("visit_description\n")
        super(QCLaTeXTranslator, self).visit_description(node)
    def visit_superscript(self, node):
        logger.info("visit_superscript\n")
        super(QCLaTeXTranslator, self).visit_superscript(node)
    def visit_subscript(self, node):
        logger.info("visit_subscript\n")
        super(QCLaTeXTranslator, self).visit_subscript(node)
    def visit_substitution_definition(self, node):
        logger.info("visit_substitution_definition\n")
        super(QCLaTeXTranslator, self).visit_substitution_definition(node)
    def visit_substitution_reference(self, node):
        logger.info("visit_substitution_reference\n")
        super(QCLaTeXTranslator, self).visit_substitution_reference(node)
    def visit_generated(self, node):
        logger.info("visit_generated\n")
        super(QCLaTeXTranslator, self).visit_generated(node)
    def visit_compound(self, node):
        logger.info("visit_compound\n")
        super(QCLaTeXTranslator, self).visit_compound(node)

        # A code block with a caption is structured like so
        # <container>
        #   <caption> </caption>
        #   <litera_block> </literal_block>
        # </container>
    def visit_container(self, node):
        logger.info("visit_container\n")
        # For now skip containers because they introduce commands defined in sphinx.sty
#        super(QCLaTeXTranslator, self).visit_container(node)
    def visit_decoration(self, node):
        logger.info("visit_decoration\n")
        super(QCLaTeXTranslator, self).visit_decoration(node)
    def visit_header(self, node):
        logger.info("visit_header\n")
        super(QCLaTeXTranslator, self).visit_header(node)
    def visit_footer(self, node):
        logger.info("visit_footer\n")
        super(QCLaTeXTranslator, self).visit_footer(node)
    def visit_docinfo(self, node):
        logger.info("visit_docinfo\n")
        super(QCLaTeXTranslator, self).visit_docinfo(node)
    def visit_comment(self, node):
        logger.info("visit_comment\n")
        super(QCLaTeXTranslator, self).visit_comment(node)
    def visit_meta(self, node):
        logger.info("visit_meta\n")
        super(QCLaTeXTranslator, self).visit_meta(node)
    def visit_system_message(self, node):
        logger.info("visit_system_message\n")
        super(QCLaTeXTranslator, self).visit_system_message(node)
    def visit_math(self, node):
        logger.info("visit_math\n")
        super(QCLaTeXTranslator, self).visit_math(node)
    def depart_start_of_file(self, node):
        logger.info("depart_start_of_file\n")
        super(QCLaTeXTranslator, self).depart_start_of_file(node)
    def depart_problematic(self, node):
        logger.info("depart_problematic\n")
        super(QCLaTeXTranslator, self).depart_problematic(node)
    def depart_topic(self, node):
        logger.info("depart_topic\n")
#        super(QCLaTeXTranslator, self).depart_topic(node)
    def depart_glossary(self, node):
        logger.info("depart_glossary\n")
        super(QCLaTeXTranslator, self).depart_glossary(node)
    def depart_productionlist(self, node):
        logger.info("depart_productionlist\n")
        super(QCLaTeXTranslator, self).depart_productionlist(node)
    def depart_production(self, node):
        logger.info("depart_production\n")
        super(QCLaTeXTranslator, self).depart_production(node)
    def depart_transition(self, node):
        logger.info("depart_transition\n")
        super(QCLaTeXTranslator, self).depart_transition(node)
    def depart_title(self, node):
        logger.info("depart_title\n")
        parent = node.parent
        if isinstance(parent, nodes.table):
            pass
            # Title of a table.
            # FIXME:  Handle titles for tables
        else:
            super(QCLaTeXTranslator, self).depart_title(node)
    def depart_subtitle(self, node):
        logger.info("depart_subtitle\n")
        super(QCLaTeXTranslator, self).depart_subtitle(node)
    def depart_desc(self, node):
        logger.info("depart_desc\n")
        super(QCLaTeXTranslator, self).depart_desc(node)
    def depart_desc_signature(self, node):
        logger.info("depart_desc_signature\n")
        super(QCLaTeXTranslator, self).depart_desc_signature(node)
    def depart_desc_signature_line(self, node):
        logger.info("depart_desc_signature_line\n")
        super(QCLaTeXTranslator, self).depart_desc_signature_line(node)
    def depart_desc_addname(self, node):
        logger.info("depart_desc_addname\n")
        super(QCLaTeXTranslator, self).depart_desc_addname(node)
    def depart_desc_type(self, node):
        logger.info("depart_desc_type\n")
        super(QCLaTeXTranslator, self).depart_desc_type(node)
    def depart_desc_returns(self, node):
        logger.info("depart_desc_returns\n")
        super(QCLaTeXTranslator, self).depart_desc_returns(node)
    def depart_desc_name(self, node):
        logger.info("depart_desc_name\n")
        super(QCLaTeXTranslator, self).depart_desc_name(node)
    def depart_desc_parameterlist(self, node):
        logger.info("depart_desc_parameterlist\n")
        super(QCLaTeXTranslator, self).depart_desc_parameterlist(node)
    def depart_desc_parameter(self, node):
        logger.info("depart_desc_parameter\n")
        super(QCLaTeXTranslator, self).depart_desc_parameter(node)
    def depart_desc_optional(self, node):
        logger.info("depart_desc_optional\n")
        super(QCLaTeXTranslator, self).depart_desc_optional(node)
    def depart_desc_annotation(self, node):
        logger.info("depart_desc_annotation\n")
        super(QCLaTeXTranslator, self).depart_desc_annotation(node)
    def depart_desc_content(self, node):
        logger.info("depart_desc_content\n")
        super(QCLaTeXTranslator, self).depart_desc_content(node)
    def depart_seealso(self, node):
        logger.info("depart_seealso\n")
        super(QCLaTeXTranslator, self).depart_seealso(node)
    def depart_rubric(self, node):
        logger.info("depart_rubric\n")
        super(QCLaTeXTranslator, self).depart_rubric(node)
    def depart_collected_footnote(self, node):
        logger.info("depart_collected_footnote\n")
        super(QCLaTeXTranslator, self).depart_collected_footnote(node)
    def depart_table(self, node):
        logger.info("depart_table\n")
        self.table = None
        self.in_table = 0
        self.body.append("\\end{longtable}\n")
#        super(QCLaTeXTranslator, self).depart_table(node)
    def depart_colspec(self, node):
        logger.info("depart_colspec\n")
#        super(QCLaTeXTranslator, self).depart_colspec(node)
    def depart_tgroup(self, node):
        logger.info("depart_tgroup\n")

    def depart_thead(self, node):
        logger.info("depart_thead\n")
        logger.info(self.table.header)
        header = self.popbody()
        self.body.extend(header)
        self.body.append("\\endfirsthead\n")
        self.body.extend(header)
        self.body.append("\\endhead\n")
        self.body.append("\\endfoot\n")
        self.body.append("\\hline\n")
        self.body.append("\\endlastfoot\n")
        self.table.in_header = False
#        super(QCLaTeXTranslator, self).depart_thead(node)
    def depart_tbody(self, node):
        logger.info("depart_tbody\n")
#        super(QCLaTeXTranslator, self).depart_tbody(node)
    def depart_row(self, node):
        logger.info("depart_row\n")
        # Reset the column number
        self.table.curr_col = -1
        # For now, we will use \\ to end a row. If we use raggedright
        # for text wrapping columns, we will need to use \tn or
        # \tabularnewline
        self.body.append(" \\tn\n")
#        super(QCLaTeXTranslator, self).depart_row(node)
    def depart_entry(self, node):
        logger.info("depart_entry\n")
        if self.table.in_header:
            context_pop = self.context.pop()
            logger.info("depart_entry: context_pop = %s" % context_pop)
            self.body.append(context_pop)
 #       super(QCLaTeXTranslator, self).depart_entry(node)
    def depart_enumerated_list(self, node):
        logger.info("depart_enumerated_list\n")
        super(QCLaTeXTranslator, self).depart_enumerated_list(node)
    def depart_list_item(self, node):
        logger.info("depart_list_item\n")
        super(QCLaTeXTranslator, self).depart_list_item(node)
    def depart_definition_list(self, node):
        logger.info("depart_definition_list\n")
        super(QCLaTeXTranslator, self).depart_definition_list(node)
    def depart_definition_list_item(self, node):
        logger.info("depart_definition_list_item\n")
        super(QCLaTeXTranslator, self).depart_definition_list_item(node)
    def depart_term(self, node):
        logger.info("depart_term\n")
        super(QCLaTeXTranslator, self).depart_term(node)
    def depart_classifier(self, node):
        logger.info("depart_classifier\n")
        super(QCLaTeXTranslator, self).depart_classifier(node)
    def depart_definition(self, node):
        logger.info("depart_definition\n")
        super(QCLaTeXTranslator, self).depart_definition(node)
    def depart_field_list(self, node):
        logger.info("depart_field_list\n")
        super(QCLaTeXTranslator, self).depart_field_list(node)
    def depart_field(self, node):
        logger.info("depart_field\n")
        super(QCLaTeXTranslator, self).depart_field(node)
    def depart_centered(self, node):
        logger.info("depart_centered\n")
        super(QCLaTeXTranslator, self).depart_centered(node)
    def depart_hlistcol(self, node):
        logger.info("depart_hlistcol\n")
        super(QCLaTeXTranslator, self).depart_hlistcol(node)
    def depart_image(self, node):
        logger.info("depart_image\n")
        super(QCLaTeXTranslator, self).depart_image(node)
    def depart_figure(self, node):
        logger.info("depart_figure\n")
        super(QCLaTeXTranslator, self).depart_figure(node)
    def depart_caption(self, node):
        logger.info("depart_caption\n")
#        super(QCLaTeXTranslator, self).depart_caption(node)
    def depart_legend(self, node):
        logger.info("depart_legend\n")
        super(QCLaTeXTranslator, self).depart_legend(node)
    # def depart_admonition(self, node):
    #     logger.info("depart_admonition\n")
    #     super(QCLaTeXTranslator, self).depart_admonition(node)
    def depart_versionmodified(self, node):
        logger.info("depart_versionmodified\n")
        super(QCLaTeXTranslator, self).depart_versionmodified(node)
    def depart_target(self, node):
        logger.info("depart_target\n")
        super(QCLaTeXTranslator, self).depart_target(node)
    def depart_attribution(self, node):
        logger.info("depart_attribution\n")
        super(QCLaTeXTranslator, self).depart_attribution(node)
    def depart_reference(self, node):
        logger.info("depart_reference\n")
        self.skip_text_in_ref = False
    def depart_download_reference(self, node):
        logger.info("depart_download_reference\n")
        super(QCLaTeXTranslator, self).depart_download_reference(node)
    def depart_pending_xref(self, node):
        logger.info("depart_pending_xref\n")
        super(QCLaTeXTranslator, self).depart_pending_xref(node)
    def depart_emphasis(self, node):
        logger.info("depart_emphasis\n")
        self.body.append('}')
    def depart_literal_emphasis(self, node):
        logger.info("depart_literal_emphasis\n")
        super(QCLaTeXTranslator, self).depart_literal_emphasis(node)
    # def depart_strong(self, node):
    #     logger.info("depart_strong\n")
    #     super(QCLaTeXTranslator, self).depart_strong(node)
    def depart_literal_strong(self, node):
        logger.info("depart_literal_strong\n")
        super(QCLaTeXTranslator, self).depart_literal_strong(node)
    def depart_abbreviation(self, node):
        logger.info("depart_abbreviation\n")
        super(QCLaTeXTranslator, self).depart_abbreviation(node)
    def depart_manpage(self, node):
        logger.info("depart_manpage\n")
        super(QCLaTeXTranslator, self).depart_manpage(node)
    def depart_title_reference(self, node):
        logger.info("depart_title_reference\n")
        super(QCLaTeXTranslator, self).depart_title_reference(node)
    def depart_citation(self, node):
        logger.info("depart_citation\n")
        super(QCLaTeXTranslator, self).depart_citation(node)
    def depart_footnote_reference(self, node):
        logger.info("depart_footnote_reference\n")
        super(QCLaTeXTranslator, self).depart_footnote_reference(node)
    def depart_line(self, node):
        logger.info("depart_line\n")
        super(QCLaTeXTranslator, self).depart_line(node)
    def depart_line_block(self, node):
        logger.info("depart_line_block\n")
        super(QCLaTeXTranslator, self).depart_line_block(node)
    def depart_block_quote(self, node):
        logger.info("depart_block_quote\n")
        super(QCLaTeXTranslator, self).depart_block_quote(node)
    def depart_option(self, node):
        logger.info("depart_option\n")
        super(QCLaTeXTranslator, self).depart_option(node)
    def depart_option_argument(self, node):
        logger.info("depart_option_argument\n")
        super(QCLaTeXTranslator, self).depart_option_argument(node)
    def depart_option_group(self, node):
        logger.info("depart_option_group\n")
        super(QCLaTeXTranslator, self).depart_option_group(node)
    def depart_option_list(self, node):
        logger.info("depart_option_list\n")
        super(QCLaTeXTranslator, self).depart_option_list(node)
    def depart_option_list_item(self, node):
        logger.info("depart_option_list_item\n")
        super(QCLaTeXTranslator, self).depart_option_list_item(node)
    def depart_description(self, node):
        logger.info("depart_description\n")
        super(QCLaTeXTranslator, self).depart_description(node)
    def depart_superscript(self, node):
        logger.info("depart_superscript\n")
        super(QCLaTeXTranslator, self).depart_superscript(node)
    def depart_subscript(self, node):
        logger.info("depart_subscript\n")
        super(QCLaTeXTranslator, self).depart_subscript(node)
    def depart_generated(self, node):
        logger.info("depart_generated\n")
        super(QCLaTeXTranslator, self).depart_generated(node)
    def depart_compound(self, node):
        logger.info("depart_compound\n")
        super(QCLaTeXTranslator, self).depart_compound(node)
    def depart_container(self, node):
        logger.info("depart_container\n")
#        super(QCLaTeXTranslator, self).depart_container(node)
    def depart_decoration(self, node):
        logger.info("depart_decoration\n")
        super(QCLaTeXTranslator, self).depart_decoration(node)
    def depart_Text(self, node):
        logger.info("depart_Text\n")
        super(QCLaTeXTranslator, self).depart_Text(node)
    def depart_system_message(self, node):
        logger.info("depart_system_message\n")
        super(QCLaTeXTranslator, self).depart_system_message(node)

class QCLaTeXBuilder(LaTeXBuilder):

    name = "qc_latex"
    format = "latex"

    def init(self):
        self.default_translator_class = QCLaTeXTranslator
        super(QCLaTeXBuilder, self).init()

# This is a setup function that sets up our plugin.
# It adds a custom builder (implemented by the QCLaTeXBuilder class)
# that simply converts rst files in our documentation to latex files
# in a format that is acceptable to the tech pubs team.
def setup(app):
    app.add_builder(QCLaTeXBuilder)
    latex_templates = os.path.abspath("source/_latex_templates")
    app.add_config_value('rst2qclatex_templates', latex_templates, 'env')
    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
