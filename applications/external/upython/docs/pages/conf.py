import datetime
import pathlib
import sys

base = pathlib.Path(__file__).parent.parent.parent
root = base.__str__()
flipperzero = base.joinpath('flipperzero').__str__()
now = datetime.datetime.now()

sys.path.append(root)
sys.path.append(flipperzero)

project = 'uPython'
copyright = str(now.year) + ', Oliver Fabel'
author = 'Oliver Fabel'
release = '1.1.0'
version = '1.1'
language = 'en'

extensions = [
    'sphinx.ext.autodoc',
    'myst_parser'
]
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown'
}

templates_path = [
    'templates'
]
exclude_patterns = []
include_patterns = [
    '**'
]

html_theme = 'alabaster'
html_theme_options = {
    'show_powered_by': False,
    'extra_nav_links': {
        'Source Code': 'https://www.github.com/ofabel/mp-flipper',
        'Bugtracker': 'https://www.github.com/ofabel/mp-flipper/issues',
        'Releases': 'https://lab.flipper.net/apps/upython'

    }
}
html_copy_source = False
html_show_copyright = False
html_show_sphinx = False
html_static_path = [
    'static'
]
html_logo = 'assets/logo.png'
html_favicon = 'assets/favicon.png'

autodoc_default_options = {
    'member-order': 'bysource',
}

add_module_names = True

maximum_signature_line_length = 50