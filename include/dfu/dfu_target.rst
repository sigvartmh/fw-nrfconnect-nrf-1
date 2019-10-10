.. _lib_dfu_target:

DFU Target
##########

The DFU Target library provides a common API for different types of firmware upgrades.
It is used to allow components dealing with firmware upgrades to only operate against a single interface.

To initialize the DFU Target library, one must know the type of firmware upgrade.
All supported firmware upgrade types can be identified by sending the first fragment of the firmware to the function :cpp:func:`dfu_target_img_type`
The result of this call can then be given as input to the :cpp:func:`dfu_target_init` function.


Supported variants
******************
The supported variants must implement a set of functions defined in :file:`subsys/dfu/src/dfu_target.c`.
Currently supported variants are described below.

MCUBoot style upgrades
======================
This variant will write the data given to the :cpp:func:`dfu_target_write` function into the secondary slot specified by MCUBoot's flash partitions. When the complete transfer is done calling the :cpp:func:`dfu_target_done` it will mark the firmware which is now stored in the secondary slot as ready to be booted on next reboot.

Modem firmware  upgrades
========================
This variant will open a socket into the modem and pass the data given to the :cpp:func:`dfu_target_write` function through the socket. If an old firmware patch recides in the modem it will delete the old firmware patch to make space for the new firmware patch. When the complete transfer is done calling :cpp:func:`dfu_target_done` will request the modem to try to apply the patch on the next reboot and close the socket.

API documentation
*****************

| Header file: :file:`include/dfu/dfu_target.h`
| Source files: :file:`subsys/dfu/src/`

.. doxygengroup:: dfu_target
   :project: nrf
   :members:
