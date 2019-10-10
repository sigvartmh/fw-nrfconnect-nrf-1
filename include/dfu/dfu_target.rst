.. _lib_dfu_target:

DFU Target
##########

The DFU Target library provides a common API for different types of firmware upgrades.
It is used to allow components dealing with firmware updates to only operate against a single interface.

To initialize the DFU Target library, one must know the type of firmware upgrade.
All supported firmware upgrade types can be identified by sending the first fragment of the firmware to the function ``dfu_target_img_type()``
The result of this call can then be given as input to the ``dfu_target_init`` function.


Supported variants
******************
The supported variants must implement a set of functions defined in :file:`subsys/dfu/src/dfu_target.c`.
Currently supported variants are described below.

MCUBoot style upgrades
======================

Modem firmware  upgrades
========================

API documentation
*****************

| Header file: :file:`include/dfu/dfu_target.h`
| Source files: :file:`subsys/dfu/src/`

.. doxygengroup:: dfu_target
   :project: nrf
   :members:
