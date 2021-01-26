.. _crypto_aes_cbc:

Crypto: AES CBC
###############

.. contents::
   :local:
   :depth: 2

Overview
********

The AES sample shows how to perform AES encryption and decryption operations using the CBC mode without padding using a 128-bit AES key.

First, the sample encrypts a sample plaintext:
   #. The Platform Security Architecture (PSA) API is initialized.
   #. The AES key used for encryption is imported to the PSA keystore.
   #. A random initialization vector (IV) is generated.
   #. Encryption is performed.
   #. The key is removed from the PSA keystore.

Then, the sample decrypts the sample plaintext:
   #. The Platform Security Architecture (PSA) API is initialized
   #. The AES key used for decryption is imported to the PSA keystore.
   #. The random initialization vector (IV) generated during the encryption is set.
   #. Decryption is performed.
   #. The key is removed from the PSA keystore.

Requirements
************

The sample supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows:  nrf5340pdk_nrf5340_cpuappns, nrf5340dk_nrf5340_cpuappns, nrf9160dk_nrf9160ns


Building and running
********************

.. |sample path| replace:: :file:`samples/crypto/aes_cbc`

.. include:: /includes/build_and_run.txt

Testing
=======

Follow these steps to test the AES CBC example:

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
