# Test fixtures

Small files the tests read. Never regenerate a golden file to make a test pass (see CONTRIBUTING.md).

| File | Origin |
|---|---|
| `session_phone.golden`, `wireless_phone.golden`, `kombi-golden.txt` | Written by the tests of this project: every byte sent to the phone and to the cluster |
| `compositor-entryevo.conf` | Copy of `/etc/wayland/compositor.conf` of a BMW EntryNav2 head unit; the test reads its `genivi-flip-renderorder` line |
| `ilm-scene/` | Output of `LayerManagerControl` on the same head unit: screen, layer and surfaces |
| `spp-bmw-uuids.txt` | The Bluetooth serial port UUID pair as the head unit holds it, in hexadecimal |
| `gps_e7/` | `LocationData.proto` of aasdk (GPL-3.0), see its own README |

The car readings hold no personal data: no serial number, no address, no position.
