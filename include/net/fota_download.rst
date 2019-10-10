.. _lib_fota_download:

FOTA download
#############

The firmware over-the-air (FOTA) download library provides functions for downloading a firmware file as an upgrade candidate to the secondary slot of :doc:`mcuboot:index`.

The library uses the :ref:`lib_download_client` to download the image.
Once the download has been started, all received data fragments are passed to the flash image library (``include/dfu/flash_img.h`` in the Zephyr repository).
The flash image library stores the data fragments at the appropriate memory address for a firmware upgrade candidate.

When the download client sends the event indicating that the download has completed, the last data fragments are flushed to persistent memory.
The received firmware is tagged as an upgrade candidate, and the download client is instructed to disconnect from the server.

By default, the FOTA download library uses HTTP for downloading the firmware file.
To use HTTPS instead, apply the changes described in :ref:`the HTTPS section of the download client documentation <download_client_https>` to the library.

The FOTA download library is used in the :ref:`http_application_update_sample` sample.


API documentation
*****************

| Header file: :file:`include/net/fota_download.h`
| Source files: :file:`subsys/net/lib/fota_download/src/`

.. doxygengroup:: fota_download
   :project: nrf
   :members:
