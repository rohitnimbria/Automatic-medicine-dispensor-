# Circuit Connections

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| Servo Signal | GPIO 19 | Orange wire |
| Servo VCC | 5V (VIN) | Red wire — use external 5V if servo causes resets |
| Servo GND | GND | Brown wire |
| Buzzer (+) | GPIO 18 | |
| Buzzer (-) | GND | |
| Button SET | GPIO 25 | Other leg → GND (INPUT_PULLUP) |
| Button UP | GPIO 26 | Other leg → GND (INPUT_PULLUP) |
| Button DOWN | GPIO 27 | Other leg → GND (INPUT_PULLUP) |
| LCD SDA | GPIO 21 | |
| LCD SCL | GPIO 22 | |
| LCD VCC | 3.3V | |
| LCD GND | GND | |

## Wiring Diagram

```
                    ┌──────────────────────┐
                    │      ESP32           │
                    │                      │
                    │  GPIO19 ────────────>│ Servo Signal
                    │  VIN (5V) ──────────>│ Servo VCC
                    │                      │
                    │  GPIO18 ────────────>│ Buzzer (+)
                    │                      │
                    │  GPIO25 ←────────────│ Button SET ── GND
                    │  GPIO26 ←────────────│ Button UP  ── GND
                    │  GPIO27 ←────────────│ Button DOWN ─ GND
                    │                      │
                    │  3.3V ──────────────>│ LCD VCC
                    │  GPIO21 (SDA) ──────>│ LCD SDA
                    │  GPIO22 (SCL) ──────>│ LCD SCL
                    │  GND ────────────────│ LCD GND
                    │  GND <───────────────│ Servo GND
                    │  GND <───────────────│ Buzzer (-)
                    └──────────────────────┘
```

## Notes
- Buttons use internal pull-up (connect other pin to GND)
- If servo causes ESP32 resets, use separate 5V supply for servo with common GND
- LCD address: 0x27 (try 0x3F if not working)
