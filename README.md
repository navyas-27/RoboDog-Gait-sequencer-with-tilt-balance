# RoboDog Level 2: Gait Sequencer with Tilt Balance

**Wokwi project:** https://wokwi.com/projects/478318137608226817  
**Code:** `sketch.ino`  
**Demo recording:** https://youtu.be/N6n60PN4R2g 

## What it does

An ESP32 drives 8 servos (hip + knee on each of 4 legs) and reads an MPU6050 motion sensor.

- **Non-blocking:** there is no `delay()` anywhere. Everything runs on `millis()` timers, so commands, the sensor and the legs never block each other.
- **Trot gait:** diagonal pairs move together (front-left + back-right, then front-right + back-left).
- **Tilt sensing:** pitch and roll are calculated from the MPU6050 with a complementary filter (98% gyro, 2% accelerometer, updated at 50 Hz).
- **Self-leveling:** in stand mode, the knee angles are adjusted from the pitch and roll so the body stays level when the sensor is tilted.
- **Serial commands** (115200 baud):

| Key | Action |
|---|---|
| `w` | walk (trot) |
| `s` | stop (legs freeze) |
| `t` | stand tall (and self-level) |
| `c` | crouch |

## Wiring

| Part | ESP32 pin |
|---|---|
| Front-left hip / knee | GPIO 13 / 14 |
| Back-left hip / knee | GPIO 18 / 19 |
| Front-right hip / knee | GPIO 23 / 25 |
| Back-right hip / knee | GPIO 26 / 27 |
| MPU6050 SDA / SCL | GPIO 21 / 22 |
| Servo power (red) | 5V (VIN) |
| MPU6050 VCC | 3V3 |

All grounds are common.

## Written answers

### 1. Power budget (12 x MG996R)

**Assumptions** (typical MG996R figures at 6 V, confirm with the supplier's datasheet): about 0.5 A per servo when moving under load, and about 2.5 A at stall.

| Case | Calculation | Servo current at 6 V | Power |
|---|---|---|---|
| Normal (all 12 moving under load) | 12 x 0.5 A | 6 A | 36 W |
| Peak (4 servos near stall while lifting, 8 normal) | 4 x 2.5 + 8 x 0.5 | 14 A | 84 W |
| Absolute worst case (all stalled, not designed for) | 12 x 2.5 A | 30 A | 180 W |

Electronics: Jetson Orin Nano about 15 W, ESP32 + MPU6050 about 1 W, so about 16 W.

**Battery side** (assuming 90% regulator efficiency, 3S pack at 11.1 V nominal):
- Normal: (36 + 16) / 0.9 = 57.8 W, and 57.8 / 11.1 = **about 5.2 A**
- Peak: (84 + 16) / 0.9 = 111 W, and 111 / 11.1 = **about 10 A**

**Battery choice: 3S LiPo, 4000 mAh, 20C or better.**
- 3S rather than 2S: the Jetson dev kit needs 9 to 19 V input, and a 2S pack (at most 8.4 V) is below that. 3S also draws less current for the same power.
- Capacity: for a 30 minute run at 5.2 A, we need 5.2 x 0.5 h = 2.6 Ah. Using only 80% of the pack gives 2.6 / 0.8 = 3.25 Ah, so a 4000 mAh pack is chosen. Estimated runtime is 4 x 0.8 / 5.2, about 37 minutes.
- Discharge rating: 4 Ah x 20C = 80 A, far above the 10 A peak.

**Regulators:**
- **Servos:** two 6 V buck converters (UBEC), each rated 10 A or more, one for each side of the body (6 servos each: about 3 A normal and 7 A peak per side). The output is set to 6.0 V, which is within the MG996R's 4.8 to 7.2 V range.
- **Logic:** a separate 5 V, 3 A buck converter for the ESP32, so servo current spikes cannot reset it. The Jetson runs directly from the 3S pack.
- **Protection:** a 15 A fuse, a main switch, a 1000 uF capacitor across each servo rail, and a common ground for everything.

### 2. ESP32 and Jetson Orin Nano UART link

**Physical link:** UART at 115200 baud, 8N1, 3.3 V logic on both sides. TX of one goes to RX of the other, with a common GND.

**Message format:** one ASCII text line per message, ending with `\n`:

`$<TYPE>,<field>,<field>,...*<CS>\n`

`<CS>` is the **XOR of every character between `$` and `*`**, written as 2 hex digits. The receiver recalculates it and **drops the message if it does not match**.

**Jetson to ESP32 (commands):** `$CMD,<action>,<speed>,<turn>*<CS>`
- `action`: `W` walk, `S` stop, `T` stand tall, `C` crouch
- `speed`: 0 to 100 (percent)
- `turn`: -100 to +100 (negative = left)
- Example: `$CMD,W,60,-20*18` means walk at 60% speed, turning left 20%.

**ESP32 to Jetson (IMU data, 50 times per second):** `$IMU,<pitch>,<roll>*<CS>` (degrees)
- Example: `$IMU,2.4,-1.1*7A`

**Fail-safe:** if the ESP32 receives no valid command for 500 ms, it stops the legs.
