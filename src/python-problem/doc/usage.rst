.. _usage:

Usage examples
==============

Creating new problem
--------------------

.. literalinclude:: ../examples/daemon_example.py

Creating problem for different executable
-----------------------------------------

.. literalinclude:: ../examples/selinux_example.py

Adding custom data
------------------

.. literalinclude:: ../examples/userspace_example.py

Querying problems
-----------------

.. literalinclude:: ../examples/list_example.py

Querying all problems
---------------------
The ``list`` method used with ``auth=True`` parameter
will try to authenticate via
`polkit <http://www.freedesktop.org/wiki/Software/polkit>`_ to
gain access to all problems on the system.

If there is no authentication agent running or authentication
is unsuccessful, the list of problems which belong to current
user is returned (same as returned by the ``list`` method).

.. literalinclude:: ../examples/list_all_example.py

Editing existing problems
-------------------------

.. literalinclude:: ../examples/edit_example.py


Watching for new problems
-------------------------

.. literalinclude:: ../examples/watch_example.py

Watching for new problems in a thread
--------------------------------------

.. literalinclude:: ../examples/thread_watch_example.py

Getting bug numbers of problems reported to bugzilla
----------------------------------------------------

.. literalinclude:: ../examples/bugzilla_numbers.py
