# PIC16F690 Phase-Angle AC Power Controller

## 📌 Overview
This project implements a phase-angle AC power controller based on the PIC16F690 microcontroller.
The system controls an AC-powered DC motor using zero-cross detection, TRIAC triggering, and a non-blocking firmware architecture with soft-start functionality.

---

## ⚙️ Features

### 1. Power-On LED Animation
Upon applying power (PWR0/PWR1), all LEDs perform a wave-like animation sequence.

---

### 2. Zero-Cross Detection
The RB5 pin receives a signal synchronized with the AC mains (220V 50Hz / 120V 60Hz).
The firmware detects the moment when the AC waveform crosses zero (“zero-cross point”).

---

### 3. Phase-Angle Control (TRIAC)
Power regulation is achieved by generating a ~20 µs pulse on pin RC7 after each zero-cross event.

- Immediate pulse after zero-cross → maximum power
- Delayed pulse (60–70% of half-cycle) → reduced power
- No pulse → minimal power (controller only, LEDs off)

---

### 4. OFF State Behavior
In the OFF state, the controller operates in one of two modes:
- Reduced power with dim LEDs (D3, D4)
- Reduced CPU frequency with LED blinking at 1 Hz

---

### 5. Power Button (PWR)
- Press → turns the system ON
- Release → returns the system to OFF state

---

### 6. Motor Control with Soft Start

#### Power Levels:
| Level | Delay (% of half-cycle) |
|------|--------------------------|
| 5 | 45% |
| 4 | 53% |
| 3 | 61% |
| 2 | 69% |
| 1 | 78% |

#### Soft Start:
- Starts at 85% delay (low power)
- Decreases delay by 1% each half-cycle
- Stops at selected level

---

### 7. Status LEDs (D3, D4)
- ON (steady) in running state
- Reflect OFF/FAULT behavior

---

### 8. Power Level Control (SW+ / SW-)
- Adjust motor power (1–5)
- LED1–LED5 indicate level:
  - LED1 → lowest
  - LED5 → highest

---

### 9. Filter Button (FLTR)
- Press:
  - Immediately stops motor
  - Blinks LED1–LED5
- Release:
  - Restarts motor with soft start

---

### 10. Startup Power Level Override
- Default level: 3
- SW+ pressed → level 5
- SW- pressed → level 1

---

### 11. Button Logic
SW+ and SW- act only on button release.

---

## 🧠 Architecture Highlights

- Timer0 → system time base (1 ms tick)
- Timer1 → TRIAC pulse timing (~20 µs)
- Non-blocking LED control
- State machine design
- Safe zero-cross synchronization

---

## 🛠 Hardware

- MCU: PIC16F690
- TRIAC driver: MOC3061 + TRIAC
- Zero-cross detection circuit
- LED indicators and buttons

---

## 🚀 Summary

This project demonstrates:
- Phase-angle AC control
- Embedded real-time design
- Interrupt-driven architecture
- Safe power electronics interfacing
- Clean, maintainable firmware structure

---

## 📄 License
Educational / demonstration use.
