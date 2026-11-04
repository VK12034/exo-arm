# exoarm (host tools)

Python side of the [exo-arm](../README.md) project: the serial protocol, a
link object with a background reader, a live telemetry plot and a CLI.

```bash
pip install -e ".[plot,dev]"

exoarm --port COM5 status
exoarm --port COM5 monitor
exoarm --port COM5 mode hold
exoarm --port COM5 move --joint 1 --deg 45
exoarm --port COM5 record --seconds 30 --out run.csv
```

`exoarm.protocol` mirrors `firmware/include/protocol.h`. If you change one,
change the other and bump `PROTOCOL_VERSION`; the telemetry frame carries the
version so a mismatch is visible rather than silently misparsed.
