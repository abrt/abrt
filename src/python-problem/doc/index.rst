.. abrt-python documentation master file, created by
   sphinx-quickstart on Tue Dec  4 12:03:58 2012.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

abrt-python
===========

High-level API for querying, creating and manipulating
problems handled by `ABRT <https://fedorahosted.org/abrt/>`_ 
in Python.

It works on top of low-level DBus or socket API provided
by ABRT. Socket API serves only as a fallback option
for systems without new DBus problem API
as it can only handle the creation of new problems.

This project lives in the
`abrt repository <http://git.fedorahosted.org/git/abrt.git>`_ 
and is distributed under GPLv2 license.

Contents:

.. toctree::
   :maxdepth: 2

   usage
   api
   properties

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

