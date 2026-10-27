import os
import subprocess
# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "GatorStomp"
copyright = "2026, Case Zumbrum, Ajna Topic, Shan Sundal, Danush Singla"
author = "Case Zumbrum, Ajna Topic, Shan Sundal, Danush Singla"
release = "0.0.0"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ["sphinx.ext.autodoc", "breathe", "exhale"]

templates_path = ["source/_templates"]
exclude_patterns = []

breathe_projects = {"GatorStomp": "./build/doxygen/xml"}
breathe_default_project = "GatorStomp"


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "sphinx_rtd_theme"
html_static_path = ["source/_static"]

exhale_args = {
    # Required Arguments
    "containmentFolder": "./source/_doxygen_generated",  # Where Exhale will generate the .rst files
    "rootFileName": "library_root.rst",  # The main landing page for your API
    "doxygenStripFromPath": "../src",  # Strip source prefix from file paths
    # Suggested Arguments
    "rootFileTitle": "Doxygen Documentation",  # Title of the main API page
    "createTreeView": True,  # Adds a collapsible tree view sidebar (requires Sphinx theme support)
    "exhaleExecutesDoxygen": True,  # Exhale will automatically run Doxygen for you!
    "exhaleUseDoxyfile": True,
}
