# JBL Flip 7 and Smart Detection (local development)

- Native BLE speaker battery and charging state, read-only AA 11 00 request.
- JBL and the existing device driver pipeline refresh independently.
- Refresh interval menu: 5, 10, 15 (default), 30, or 60 seconds between polls.
- Smart detection: faster HID arrival/resume refresh and one prompt retry at startup or after a previously detected device misses a poll.
- JBL tries a previously verified BLE address before rescanning; on failure, falls back to discovery.
- Rapoo passive report timing remains controlled by the mouse; actual refresh times depend on driver response times.
- No upstream PR until hardware and regression validation are complete.
- Protocol reference: https://github.com/ffrafat/JBLControl/blob/main/docs/PROTOCOL-FLIP7.md
