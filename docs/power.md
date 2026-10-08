# Power measurements

Measured by the owner with a USB power meter, following spec §9.4:

- Power the board from a USB charger, not a PC. With no USB host, the firmware follows its sleep policy.
- Use a fully charged 18650, or remove it, so charging current doesn't distort the reading.
- Read the meter's mAh counter over at least 1 h per scenario.
- Battery-side current ≈ 5 V current × 5 V / V_bat. This ignores converter losses.

Many USB meters are inaccurate below 1 mA, so the long windows matter.

Baseline to beat: the vendor factory firmware draws about 90 mA at 5.3 V (AGENTS.md gotcha 12).

## Firmware facts that shape the numbers

Firmware at M2 (4704a19), measured with `sleep test` and `sleep stats` on 2026-09-28. These are tethered samples, from two awake phases per test, on wakes that push a new minute but sample no sensors:
- **Deep sleep:** 63 ms of app time per routine wake, plus ROM and bootloader time, which esp_timer doesn't see. Before the review fixes it was 168 ms: every wake started the USB console, initialised NVS and printed about 45 start-up log lines. Routine wakes (the RTC alarm or its backup timer) now skip all three.
- **Light sleep:** 57–58 ms per wake. The console runs from the cold boot; while no PC is attached its REPL polls every 10 ms during awake phases, which costs little.
- About 38 ms of either figure is the full-frame push (M1: per-pixel conversion from PSRAM plus SPI at 10 MHz). Every fifth wake also samples the SHTC3 and the battery, which adds about 40 ms (103 ms for such a deep wake).
- One wake per minute: the display updates every minute, and the sensors are sampled on every fifth.
- The panel stays in LPM at 1 Hz throughout.
- Full cycles slept 60.0 s, so nothing woke the board early.

Firmware at M3a (the dashboards, 2026-09-28): 64 ms of app time per routine deep wake, measured on the M3a spike with `sleep test deep 2`. That is about the M2 figure, since a dashboard draws and pushes in about the time the clock screen did.

Firmware at M5 (2026-10-01, the board tethered, from its logs): a sync keeps the radio on for 3.2–4.4 s on the home network at −87 to −88 dBm: the join about 2 s, SNTP 0.1–0.2 s, the forecast and the air quality about 1 s each. When the server or the weak link is slow it took 13–27 s, and one forecast timed out at 10 s; the 45 s limit caps it (spec §9.3). At the default schedule, one sync a day, that is about 0.1 mAh a day (4 s at the 100 mA the radio draws, an estimate), and 0.75 mAh on a slow day, next to the converter's floor of 11.5 mA, about 276 mAh a day. Sync mode `always` keeps the chip awake with Wi-Fi in modem sleep: its cost is the owner's measurement below.

Firmware at M6 (2026-10-02, ede8f1c, the board tethered, from its logs, the home network at −85 dBm):
- **A sync with the radar step** took 3.5 s in sync mode `times` (the join, SNTP, the forecast, the air quality and the radar, its frame already held), 4.4 s after quiet hours with one new ČHMÚ frame, and 10.3 s with RainViewer's index and tiles for a view over Berlin.
- **Sync mode `always`:** a radar-only refresh comes a minute after each 5-minute step and took 0.6–0.8 s for one ČHMÚ frame, each file on its own TLS connection; the hour, 12 frames, took 9.2 s. A sync with the flight radar polling took 2.6 s.
- **The flight radar** polls every 10 s at 50 km over one kept TLS session; its first poll after the view comes up took 25–50 s on the weak link.
- **The Rain radar preset** keeps the chip awake 287 ms per routine wake on light sleep and 346 ms on deep sleep, where the frame comes from its file, against 57–64 ms for the dashboards (M2, M3a): the map, the rain and the push, every minute.

## The hardware floor

The TPS63020 that makes the 3V3 rail has PS/SYNC tied high, which forces PWM and disables its power-save mode (AGENTS.md gotcha 23). TI's efficiency curve for that mode (datasheet Figure 9) is about 1–2 % at 0.1 mA and about 10 % at 1 mA, so the converter burns tens of mW even when the 3V3 load is almost nothing. That loss is there in every mode, deep sleep included, and on battery as well as on USB. Two deep-sleep runs, with and without the battery, both drew about 60 mW (11.4–11.5 mA at 5.2 V), while the chip was awake 0.1 % of the time. Deep and light sleep differ by less than that floor, so compare them in absolute mA.

**D3 (2026-09-28): light sleep.** Deep and light sleep both drew 11.5 mA at 5.24 V: 60.35 against 60.29 mW, a difference within the meter's resolution. The spec's rule (§3.4) picks deep sleep only if it is clearly lower, so the default stays light. A rough estimate of what the firmware itself adds: deep ≈ 0.5 mW (a ~10 µA floor plus one full boot per minute), light ≈ 0.8 mW (a ~0.2 mA floor plus short wakes). Both vanish next to the converter's loss. Deep sleep stays available (`power idle deep`), and the comparison should be repeated if the board gets the PS/SYNC rework.

## Results

| Date | Commit | Scenario | Settings | Meter | Window | mAh | Mean at 5 V | Est. battery current | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 2026-09-28 | 4704a19 | Deep sleep, idle | `power idle deep`; display every 1 min, sensors every 5 min; LPM 1 Hz; 18650 removed; wall charger | Fnirsi FNB58 | 1 h 55 min | 21.78 (0.114 Wh) | 11.4 mA at 5.23 V (meter: a fairly steady 11.18 mA) | — | The clock was invalid throughout. With no battery, moving the cable cut the power, which stopped the RTC, so the screen showed "Set time" and most wakes pushed no frame. Also includes the charger running with no battery. The battery estimate waits for the USB-path overhead (PWR-off reading) |
| 2026-09-28 | ffbf55c | Deep sleep, idle | `power idle deep`; display every 1 min, sensors every 5 min; LPM 1 Hz; 18650 in and charged (CHG LED off throughout); wall charger | Fnirsi FNB58 | 58 min | 11.14 (0.0583 Wh) | 11.5 mA at 5.24 V | — | Valid clock. The board's stats over 88 min (including this window): 86 deep sleeps, wakes rtc 85 and key 1, mean awake 71 ms, mean sleep 59.4 s. So the chip was awake 0.12 % of the time, and the ~60 mW floor comes from the hardware. Most of it is likely the 3V3 converter running in forced PWM (AGENTS.md gotcha 23) |
| 2026-09-28 | ffbf55c | Light sleep, idle | `power idle light`; otherwise as the run above | Fnirsi FNB58 | 1 h 55 min | 22.07 (0.1156 Wh) | 11.5 mA at 5.24 V | — | Valid clock. The run's sleep stats were lost, because the board was switched off (next row) before it went back to the Mac. The tethered `sleep test light` checks the same day showed 57–58 ms awake and 60.0 s asleep per cycle |
| 2026-09-28 | ffbf55c | Board switched off (PWR), USB still connected | 18650 in and charged | Fnirsi FNB58 | spot reading after ~30 s | — | 0.87 mA (4.6 mW) | — | Charger, power latch and the parts before it. The ~55.7 mW left of the sleep floors is behind the latch: the converter plus the 3V3 loads |
| — | — | M5: the energy of a sync | 10 syncs on demand, 2 min apart, against 20 min of idle | Fnirsi FNB58 | — | — | — | — | To be measured by the owner (M5 acceptance 4) |
| — | — | M5: sync mode `always` | `always`, quiet hours off; the web UI on the LAN | Fnirsi FNB58 | 1 h | — | — | — | To be measured by the owner (M5 acceptance 4) |
| — | — | M5: the default schedule | `times` 05:30, display every 1 min, sensors every 5 min; light sleep | Fnirsi FNB58 | 24 h | — | — | — | To be measured by the owner (M5 acceptance 4): the daily average |
| — | — | M6: a radar frame per sync | 10 syncs on demand, 2 min apart, against M5's syncs without the radar | Fnirsi FNB58 | — | — | — | — | To be measured by the owner (M6 acceptance 6) |
| — | — | M6: the flight radar in sync mode `always` | an hour with the Flights view, against an hour of `always` with the clock | Fnirsi FNB58 | 2 × 1 h | — | — | — | To be measured by the owner (M6 acceptance 6) |

## The T5-4.7 (fork)

The LilyGo T5-4.7 (ESP32, T5 spec) has no forced-PWM converter: its 3V3 rail is an LDO, so it idles in deep sleep (DT6). Its own figures:

**Refresh times** (2026-10-07, cc1d27e, `panel bench` and the logs, the board on USB): for the whole 960×540 panel, the LUT in internal RAM, two runs each, within 1 ms of each other unless given:
- a clean update (clear, then GC16): 2.22 s, of which the clear takes 1.37 s; a routine minute's clean update with the dashboard: 2.0–2.2 s;
- GL16: 1.10–1.13 s;
- DU: 0.55 s;
- with the LUT in PSRAM: clean 2.64 s, GL16 1.77 s, DU 0.57 s (so the LUT stays internal, T5 spec §5.1).

A routine deep-sleep minute keeps the chip awake about 2.2 s, nearly all of it the clean update (`sleep stats` over 30 cycles: mean awake 2.25 s, mean asleep 56.6 s).

**The clock's drift** (2026-10-07, the board on USB at room temperature): 30 one-minute deep-sleep cycles (`sleep test deep 30`), the board's clock against the Mac's at the start and the end, timed at the board's second edge to about 0.1 s:

| Slow clock | Build | Window | Drift | Rate |
|---|---|---|---|---|
| 150 kHz RC (ESP-IDF default) | T1 bring-up, `CONFIG_RTC_CLK_SRC_INT_RC` | 16:15:47–16:49:06 UTC | +17 to +19 s (the start set ±1 s) | about +9000 ppm, 13 min a day fast |
| 8MD256 (the T5's default) | T1 bring-up, `CONFIG_RTC_CLK_SRC_INT_8MD256` (as cc1d27e) | 16:51:28–17:23:46 UTC | −1.96 s | about −1000 ppm, 85 s a day slow |

**Current on USB** (2026-10-08, the owner's USB meter between a wall charger and the board, the LiPo charged; the clock unset, so no redraws):

| Build | State | Current at 5.03 V | Notes |
|---|---|---|---|
| cc1d27e | idle light, 12 h+ | ~61 mA, steady | the light-sleep spin (T5 gotcha T8) likely held it awake |
| 2bf1d25 | `sleep test light 15` | 138 mA | the spin: 1 light sleep in 2 h |
| 69c2369 | held awake by a console line | 107 mA | the ESP32 at 240 MHz on top of the floor |
| 69c2369 | light sleep (46–50 s a minute, 7 ms awake) | ~67 mA | flat on the meter's chart |
| 69c2369 | deep sleep (`sleep test deep 15`) | ~65 mA | the same with either shift-register rest word |

The ~65 mA is the hardware with USB in: VBUS holds the panel's switched rail on and the USB-serial chip runs (T5 spec §2.5 gotcha 20). The T5's sleep current is a battery-side measurement, still to be made (an ammeter in series with the LiPo); LilyGo quotes about 380 µA.

