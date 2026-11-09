# Independent GPS wire schema

`LocationData.proto` is an unmodified copy from opencardev/aasdk commit
`f9cf21e2fd3bfb8a43e61601c84d9b1b96389ff0`; source URL and SHA256 are in `sources.json`.
This fixture is used only by PC tests, never compiled into or shipped with the HU software.

`test_gps_wire_oracle.py` compiles it using Google's protoc, generates expected E7 messages
from literal geographic fixtures, and compares them with the actual host/ARM C++ encoder.
An encoder that sends E6 values fails this comparison by a factor of 10. The native BMW reader
stores coordinates internally in E6; they are converted to E7 at the AA wire boundary.
