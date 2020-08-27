.. _subsys_pcd:

Peripheral CPU DFU (PCD)
########################

The Peripheral CPU DFU (PCD) provides functionality for transferring DFU images from the application core to the network core on the nRF5340 SoC.

Overview
********

In the nRF5340 SoC, the application core is not capable of accessing the flash of the network core.
However, firmware upgrades for the network core is stored in the application flash.
This is because of the limited size of the network core flash.
Hence we need a mechanism where the network core can download firmware upgrades from the application flash.
The PCD library provides functionality for both application and network core in this situation.
A region of SRAM which is accessibly by both cores is used to communicate information between the cores.

On the application core, this library is used after verifying the signature of an update which is targeting the network core.
Once a network core firmware update is verified this library is used to communicate to the network core where the update can be found, and its expected SHA.
When the update instructions are written, the application core will enable the network core, and wait for a ``pcd_status`` indicating that the update has successfully completed.
The SRAM region used to communicate update instructions to the network core is locked before starting the application on the application core.
This is done to avoid compromised code from being able to perform updates of the network core firmware.

On the network core, this library is used to look for instructions on where to find the updates.
Once an update instruction is found, this library is used to perform the transfer of the firmware update.

In the application core, the PCD library is used by the (:doc:`mcuboot:index`) sample.

In the network core, the PCD library is used by the :ref:`nc_bootloader` sample.


API documentation
*****************

| Header file: :file:`include/dfu/pcd.h`
| Source files: :file:`subsys/dfu/pcd/`

.. doxygengroup:: pcd
   :project: nrf
   :members:
