.. _crypto_hmac:

Crypto: HMAC
############

.. contents::
   :local:
   :depth: 2

Overview
********

The HMAC sample shows how to perform HMAC signing and verification operations using the SHA256 hashing algorithm.

First, the sample signs a sample plaintext:
   #. The Platform Security Architecture (PSA) API is initialized.
   #. The HMAC key used for signing is imported to the PSA keystore.
   #. Signing is performed.
   #. The key is removed from the PSA keystore.

Then, the sample verifies the signagure:
   #. The Platform Security Architecture (PSA) API is initialized.
   #. The HMAC key used for verification is imported to the PSA keystore.
   #. Verification is performed.
   #. The key is removed from the PSA keystore.

Requirements
************

The sample supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows:  nrf5340dk_nrf5340_cpuappns, nrf5340dk_nrf5340_cpuappns, nrf9160dk_nrf9160ns


Building and running
********************

.. |sample path| replace:: :file:`samples/crypto/hmac`

.. include:: /includes/build_and_run.txt

Testing
=======

Follow these steps to test the HMAC example:

1. Start a terminal emulator like PuTTY and connect to the used COM port with the following UART settings:

   * Baud rate: 115.200
   * 8 data bits
   * 1 stop bit
   * No parity
   * HW flow control: None
#. Compile and program the application.
#. Observe the logs from the application using an RTT Viewer or a terminal emulator.

.. note::
   By default, the sample is configured to use both RTT and UART for logging.
   If you are using RTT, skip the first step of the testing procedure.
