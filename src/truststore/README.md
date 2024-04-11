# Full TLS Truststore

## Generate / Update

1. curl --remote-name https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/gen_crt_bundle.py
1. curl --remote-name https://curl.se/ca/cacert.pem
1. python3 gen_crt_bundle.py --input cacert.pem
