.. GatorStomp documentation master file, created by
   sphinx-quickstart on Tue Oct 20 12:49:23 2026.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

GatorStomp documentation
========================

GatorStomp is a digital guitar pedal designed for use with VST, LV2, and NAM plugins.

For working with ``rst`` syntax, look into `reStructuredText <https://www.sphinx-doc.org/en/master/usage/restructuredtext/index.html>`_.

Building the docs
-----------------

.. code-block:: bash

   python -m venv .venv
   pip install -r requirements.dev.txt
   cd docs
   make html

.. note::

   This will create an `index.html` file in `docs/build`

.. toctree::
   :maxdepth: 2
   :caption: Contents:

