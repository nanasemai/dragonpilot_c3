# ALKA (Always-on Lane Keeping Assist) Design v2

## Overview

ALKA enables lateral control (steering) when ACC Main is ON, without requiring cruise to be engaged. This allows lane keeping assist to function independently of longitudinal control.

---

## Per-Brand Summary

| Brand | Status | Tracks | ACC Main Source | Button Source | Notes |
|-------|--------|--------|-----------------|---------------|-------|
| Body | Disabled | - | - | - | No steering capability |
| Chrysler | Disabled | - | - | - | Needs special handling |
| Ford | Enabled | Main + TJA | EngBrakeData (0x165) CcStat | Steering_Data_FD1 (0x083) bit 40 | |
| GM | Disabled | - | - | - | No ACC Main signal |
| Honda Nidec | Enabled | Main | SCM_FEEDBACK (0x326) MAIN_ON | - | |
| Honda Bosch | Enabled | Main | SCM_FEEDBACK (0x326) MAIN_ON | - | |
| Hyundai | Enabled | Main + LKAS | SCC11 (0x420) bit 0 | BCM_PO_11 (0x391) bit 4 | |
| Hyundai CAN-FD | Enabled | Main + LKAS | SCC_CONTROL (0x1A0) bit 66 | LFA button | |
| Mazda | Enabled | Main | - | - | |
| Nissan | Enabled | Main | - | - | |
| PSA | Disabled | - | - | - | Not implemented |
| Rivian | Disabled | - | - | - | Different architecture |
| Subaru | Enabled | Main | - | - | |
| Subaru Preglobal | Enabled | Main | - | - | |
| Tesla | Disabled | - | - | - | Different architecture |
| Toyota | Enabled | Main | PCM_CRUISE_2 (0x1D3) | - | |
| Toyota (UNSUPPORTED_DSU) | Enabled | Main | DSU_CRUISE (0x365) | - | |
| VW MQB | Enabled | Main | - | - | |
| VW PQ | Enabled | Main | - | - | |

**Main**: `lkas_on = cruiseState.available` (direct assignment)
**Main + Button**: Button toggles lkas_on, ACC main acts as master disable only (falling edge disables, rising edge does NOT enable)

---

## Permission Model

Lateral control requires checks at both layers. Normal path uses `controls_allowed`, ALKA path uses additional checks.

| Check | Panda | openpilot | Notes |
|-------|:-----:|:---------:|-------|
| **Normal Path** |
| `controls_allowed` (cruise engaged) | ✓ | ✓ | Either this OR ALKA path |
| **ALKA Path** |
| `alka_allowed` (brand supports) | ✓ | ✓ | Set per brand in safety init |
| `ALT_EXP_ALKA` (user enabled) | ✓ | ✓ | alternativeExperience flag |
| `lkas_on` (ACC Main / LKAS button) | ✓ | ✓ | Tracked via CAN messages |
| `vehicle_moving` / `!standstill` | ✓ | ✓ | |
| `!steering_disengage` (no override) | ✓ | ✗ | Safety layer only |
| **openpilot Additional** |
| `gear_ok` (not P/N/R) | ✗ | ✓ | Python layer only |
| `calibrated` | ✗ | ✓ | Python layer only |
| `seatbelt latched` | ✗ | ✓ | Python layer only |
| `doors closed` | ✗ | ✓ | Python layer only |
| `!steerFaultTemporary` | ✗ | ✓ | Python layer only |
| `!steerFaultPermanent` | ✗ | ✓ | Python layer only |

---

## Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                         CAN Bus                                      │
└─────────────────────────────────────────────────────────────────────┘
                    │                              │
                    ▼                              ▼
┌─────────────────────────────────┐  ┌─────────────────────────────────┐
│  Safety Layer (panda C code)    │  │  Python Layer                   │
│                                 │  │                                 │
│  rx_ext_hook:                   │  │  carstate.py:                   │
│  - Parse ACC Main signal        │  │  - Parse ACC Main signal        │
│  - Parse LKAS button            │  │  - Parse LKAS button            │
│  - Update lkas_on               │  │  - Update self.lkas_on          │
│                                 │  │                                 │
│  lat_control_allowed():         │  └─────────────┬───────────────────┘
│  - Check lkas_on + other flags  │                │
│  - Gate steering commands       │                ▼
└─────────────────────────────────┘  ┌─────────────────────────────────┐
                                     │  card.py:                       │
                                     │  - Publish carStateExt.lkasOn   │
                                     └─────────────┬───────────────────┘
                                                   │
                                                   ▼
                                     ┌─────────────────────────────────┐
                                     │  controlsd.py:                  │
                                     │  - Read carStateExt.lkasOn      │
                                     │  - Check ALKA conditions        │
                                     │  - Set CC.latActive             │
                                     └─────────────────────────────────┘
```

### Key Files

| File | Purpose |
|------|---------|
| `custom.capnp` | Defines `CarStateExt` struct with `lkasOn` field |
| `log.capnp` | Includes `carStateExt` in event union |
| `interfaces.py` | Defines `self.lkas_on = False` default in `CarStateBase` |
| `carstate.py` (per brand) | Tracks `lkas_on` based on ACC Main + LKAS button |
| `card.py` | Publishes `carStateExt.lkasOn` from `CI.CS.lkas_on` |
| `controlsd.py` | Reads `carStateExt.lkasOn` to determine `alka_active` |

---

## ACC Main Tracking

Each brand tracks ACC Main differently. The tracking code must be guarded:

```c
if (alka_allowed && (alternative_experience & ALT_EXP_ALKA)) {
  // Track ACC main state here
}
```

This guard ensures:
1. Brand supports ALKA (`alka_allowed`)
2. User enabled ALKA (`ALT_EXP_ALKA`)

Without both conditions, no ACC Main tracking occurs, and ALKA remains disabled.

---

## Testing

Safety tests verify:
- `alka_allowed` flag set correctly per brand
- ACC Main tracking updates `lkas_on`
- `lat_control_allowed()` returns true only when all conditions met
- Steering TX blocked when ALKA conditions not met
- Bus routing variants (camera_scc, alt_eps, unsupported_dsu)
