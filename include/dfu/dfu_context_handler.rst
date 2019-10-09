.. _lib_dfu_context_handler:

DFU Context Handler
###################

The DFU Context Handler library provides a common API for different types of firmware upgrades.
It is used to allow components dealing with firmware updates to only operate against a single interface.

To initialize the DFU Context handler, one must know the type of firmware upgrade.
All supported firmware upgrade types can be identified by sending the first fragment of the firmware to the function ``dfu_ctx_img_type()``
The result of this call can then be given as input to the ``dfu_ctx_init`` function.


Supported variants
******************
The supported variants must implement a set of functions defined in :file:`subsys/dfu/src/dfu_context_handler.c`.
Currently supported variants are described below.

MCUBoot style upgrades
======================

Modem firmware  upgrades
========================

API documentation
*****************

| Header file: :file:`include/dfu/dfu_context_handler.h`
| Source files: :file:`subsys/dfu/src/`

.. doxygengroup:: dfu_ctx_handler
   :project: nrf
   :members:
