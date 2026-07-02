#!/bin/bash
# Layout-bisection trial: build psa_tls client with a given main stack size,
# run the TLS handshake against a local openssl server, report PASS/HANG.
SIZE=$1
EXTRA=$2
LABEL="$SIZE${EXTRA:+ $EXTRA}"
D=/home/siho/dev/Nordic/ncs/nrf/samples/crypto/psa_tls
SCRATCH=/tmp/claude-1001/-home-siho-dev-Nordic-ncs-nrf/66a46612-5cab-4029-910e-2a5567ca520b/scratchpad
UARTLOG=$SCRATCH/screen_598312.pts-7.zeno.log
cd $D || exit 1

west build -p -b nrf54h20dk/nrf54h20/cpuapp --sysbuild -d build_bisect . -- \
  -DEXTRA_CONF_FILE="overlays/client.conf;overlays/ecdsa_secp256r1.conf;overlays/tls_1_3_ecdsa.conf;overlays/cracen.conf" \
  -Dpsa_tls_CONFIG_MAIN_STACK_SIZE=$SIZE $EXTRA > $SCRATCH/bisect_build_$SIZE.log 2>&1
if [ $? -ne 0 ]; then echo "$LABEL BUILD_FAIL"; exit 1; fi

STACK=$(nm build_bisect/psa_tls/zephyr/zephyr.elf | grep " z_main_stack" | awk '{print $1}')

sudo pkill -f "eth-rtt-link-rs --elf" 2>/dev/null
pkill -f "s_server -accept" 2>/dev/null
sleep 4

west flash -d build_bisect --dev-id 1051107225 > $SCRATCH/bisect_flash.log 2>&1
if [ $? -ne 0 ]; then
  sleep 5
  west flash -d build_bisect --dev-id 1051107225 > $SCRATCH/bisect_flash.log 2>&1
  if [ $? -ne 0 ]; then echo "$LABEL stack@$STACK FLASH_FAIL"; exit 1; fi
fi

rm -f $SCRATCH/bisect_ossl.log
(sleep infinity | openssl s_server -accept 4243 -tls1_3 -msg \
  -cert certs/ecdsa/secp256r1/cert.pem -key certs/ecdsa/secp256r1/cert.key \
  -ciphersuites TLS_AES_256_GCM_SHA384 > $SCRATCH/bisect_ossl.log 2>&1) &
OSSL_PID=$!

> $UARTLOG

sudo ./eth-rtt-link-rs/target/release/eth-rtt-link-rs \
  --elf build_bisect/psa_tls/zephyr/zephyr.elf --ipv4 192.0.2.1 \
  > $SCRATCH/bisect_bridge.log 2>&1 &
sleep 4

nrfutil device reset --serial-number 1051107225 > /dev/null 2>&1

RESULT=TIMEOUT
for i in $(seq 1 24); do
  sleep 5
  if grep -q "handshake completed" $UARTLOG 2>/dev/null; then
    RESULT=PASS
    break
  fi
  if grep -q "Finished" $SCRATCH/bisect_ossl.log 2>/dev/null; then
    sleep 15
    if grep -q "handshake completed" $UARTLOG 2>/dev/null; then
      RESULT=PASS
    else
      RESULT=HANG
    fi
    break
  fi
done

sudo pkill -f "eth-rtt-link-rs --elf" 2>/dev/null
kill $OSSL_PID 2>/dev/null
pkill -f "s_server -accept" 2>/dev/null

echo "$LABEL stack@$STACK $RESULT"
