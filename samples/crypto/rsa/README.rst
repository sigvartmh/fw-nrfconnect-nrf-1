.. _crypto_rsa:

Crypto: RSA
###########

.. contents::
   :local:
   :depth: 2

Overview
********

The RSA sample shows how to perform singing and verification of a sample plaintext using a 1024-bit key.

First, the sample signs a sample plaintext:
   #. The Platform Security Architecture (PSA) API is initialized.
   #. An RSA keypair is generated and imported to the PSA keystore.
   #. Signing is performed.
   #. The public key is exported.
   #. The keypair is removed from the PSA keystore.

Then, the sample verifies the signature:
   #. The Platform Security Architecture (PSA) API is initialized
   #. The RSA public key is imported to the PSA keystore.
   #. Verification of the signature is performed.
   #. The key is removed from the PSA keystore.

Requirements
************

The sample supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows:  nrf5340pdk_nrf5340_cpuappns, nrf5340dk_nrf5340_cpuappns, nrf9160dk_nrf9160ns


Building and running
********************

.. |sample path| replace:: :file:`samples/crypto/rsa`

.. include:: /includes/build_and_run.txt

Testing
=======

Follow these steps to test the RSA example:

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
   If you are using RTT, skip the first step of the testing procedure
