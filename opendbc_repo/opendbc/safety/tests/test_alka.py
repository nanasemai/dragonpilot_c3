#!/usr/bin/env python3
"""
Tests for ALKA (Always-on Lane Keeping Assist) feature.

ALKA allows lateral control when:
1. alka_allowed is set for the brand
2. ALT_EXP_ALKA flag is set in alternative_experience
3. lkas_on is true (ACC Main / LKAS button ON)
4. vehicle_moving is true
5. steering_disengage is false (no driver override)
"""
import unittest
import numpy as np

from opendbc.car.structs import CarParams
from opendbc.car.toyota.values import ToyotaSafetyFlags
from opendbc.safety import ALTERNATIVE_EXPERIENCE
from opendbc.safety.tests.libsafety import libsafety_py
from opendbc.safety.tests.common import CANPackerSafety, make_msg


class TestALKABase(unittest.TestCase):
  """Base test class for ALKA functionality."""

  TX_MSGS = []  # Required by test_tx_hook_on_wrong_safety_mode

  safety: libsafety_py.LibSafety
  packer: CANPackerSafety

  @classmethod
  def setUpClass(cls):
    if cls.__name__ == "TestALKABase":
      raise unittest.SkipTest("Base class")

  def _reset_safety_hooks(self):
    self.safety.set_safety_hooks(self.safety.get_current_safety_mode(),
                                 self.safety.get_current_safety_param())

  def _rx(self, msg):
    return self.safety.safety_rx_hook(msg)

  def _tx(self, msg):
    return self.safety.safety_tx_hook(msg)

  def _set_vehicle_moving(self, moving: bool):
    """Override in subclass to set vehicle moving state via CAN message."""
    raise NotImplementedError

  def _torque_cmd_msg(self, torque, steer_req=1):
    """Override in subclass to create torque command message."""
    raise NotImplementedError


class TestALKAToyota(TestALKABase):
  """Test ALKA functionality for Toyota.

  Note: Toyota safety uses PCM_CRUISE_2 (0x1D3) or DSU_CRUISE (0x365) for ACC main.
  The tests use raw CAN packets to avoid checksum issues with the CANPacker.
  """

  def setUp(self):
    self.packer = CANPackerSafety("toyota_nodsu_pt_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.toyota, 73)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {("WHEEL_SPEED_%s" % n): speed * 3.6 for n in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_safety("WHEEL_SPEEDS", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEER_TORQUE_CMD": torque, "STEER_REQUEST": steer_req}
    return self.packer.make_can_msg_safety("STEERING_LKA", 0, values)

  def _torque_meas_msg(self, torque):
    values = {"STEER_TORQUE_EPS": (torque / 73) * 100.}
    return self.packer.make_can_msg_safety("STEER_TORQUE_SENSOR", 0, values)

  def _pcm_cruise_msg(self, main_on, cruise_active=False):
    values = {"MAIN_ON": main_on, "CRUISE_ACTIVE": cruise_active, "GAS_RELEASED": True}
    return self.packer.make_can_msg_safety("PCM_CRUISE", 0, values)

  def _acc_main_msg(self, main_on):
    """Create raw ACC main message for Toyota (PCM_CRUISE_2 0x1D3, bit 15)."""
    # PCM_CRUISE_2 is addr 0x1D3 = 467, MAIN_ON is bit 15 (byte 1, bit 7)
    dat = bytearray(8)
    if main_on:
      dat[1] = 0x80  # bit 15 = byte 1, bit 7
    # Calculate checksum: sum of address bytes + data bytes
    s = 8  # length
    s += 0x1D3 & 0xFF  # low byte of address
    s += (0x1D3 >> 8) & 0xFF  # high byte of address
    for i in range(7):
      s += dat[i]
    dat[7] = s & 0xFF
    return libsafety_py.make_CANPacket(0x1D3, 0, bytes(dat))

  def _set_prev_torque(self, t):
    self.safety.set_desired_torque_last(t)
    self.safety.set_rt_torque_last(t)
    self.safety.set_torque_meas(t, t)

  def test_alka_allowed_for_toyota(self):
    """Verify alka_allowed flag is set in Toyota safety mode init."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """Verify lkas_on tracks ACC main state (on/off) for Toyota."""
    # Initially off
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())

    # Turn on
    self._rx(self._acc_main_msg(1))
    self.assertTrue(self.safety.get_lkas_on())
    self.assertTrue(self.safety.get_acc_main_on())

    # Turn off
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())

  def test_alka_lat_control_allowed_conditions(self):
    """Verify lat_control_allowed requires all ALKA conditions: flag + lkas_on + moving."""
    # Reset state
    self._reset_safety_hooks()
    self.safety.set_controls_allowed(False)

    # Without ALKA flag, lat_control_allowed = controls_allowed
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.DEFAULT)
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.assertFalse(self.safety.get_lat_control_allowed())

    # With ALKA flag but vehicle not moving
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self._set_vehicle_moving(False)
    self.assertFalse(self.safety.get_lat_control_allowed())

    # With ALKA flag but lkas_on is false
    self._set_vehicle_moving(True)
    self._rx(self._acc_main_msg(0))  # ACC main off
    self.assertFalse(self.safety.get_lat_control_allowed())

    # All conditions met: ALKA flag + lkas_on + vehicle_moving
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.assertTrue(self.safety.get_lat_control_allowed())

  def test_alka_allows_steering_without_controls_allowed(self):
    """Verify torque TX is allowed via ALKA even when controls_allowed=false."""
    self._reset_safety_hooks()

    # Set up ALKA conditions
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.safety.set_controls_allowed(False)

    # Should be able to send small torque
    self._set_prev_torque(0)
    for _ in range(6):
      self._rx(self._torque_meas_msg(0))
    self.assertTrue(self._tx(self._torque_cmd_msg(10, steer_req=1)))

  def test_alka_disabled_when_acc_main_off(self):
    """Verify torque TX is blocked when ACC main turns off."""
    self._reset_safety_hooks()

    # Set up ALKA conditions
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.safety.set_controls_allowed(False)

    # Turn off ACC main
    self._rx(self._acc_main_msg(0))

    # Should NOT be able to send torque
    self._set_prev_torque(0)
    for _ in range(6):
      self._rx(self._torque_meas_msg(0))
    self.assertFalse(self._tx(self._torque_cmd_msg(10, steer_req=1)))

  def test_alka_disabled_when_vehicle_stopped(self):
    """Verify torque TX is blocked when vehicle stops (safety requirement)."""
    self._reset_safety_hooks()

    # Set up ALKA conditions
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.safety.set_controls_allowed(False)

    # Vehicle stops
    self._set_vehicle_moving(False)

    # Should NOT be able to send torque (except 0)
    self._set_prev_torque(0)
    for _ in range(6):
      self._rx(self._torque_meas_msg(0))
    self.assertFalse(self._tx(self._torque_cmd_msg(10, steer_req=1)))
    # Zero torque should always be allowed
    self.assertTrue(self._tx(self._torque_cmd_msg(0, steer_req=1)))

  def test_alka_reset_on_safety_init(self):
    """Verify lkas_on resets to false on safety mode re-initialization."""
    self.safety.set_lkas_on(True)
    self.assertTrue(self.safety.get_lkas_on())

    # Reset safety hooks
    self._reset_safety_hooks()

    # lkas_on should be reset to false
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_disabled_when_steering_disengage(self):
    """Verify ALKA disables on driver steering override (hands on wheel)."""
    self._reset_safety_hooks()

    # Set up ALKA conditions
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self._rx(self._acc_main_msg(1))  # ACC main on
    self._set_vehicle_moving(True)
    self.safety.set_controls_allowed(False)

    # Verify ALKA is working
    self.assertTrue(self.safety.get_lat_control_allowed())

    # Set steering_disengage (driver override)
    self.safety.set_steering_disengage(True)

    # Should NOT have lat control
    self.assertFalse(self.safety.get_lat_control_allowed())

    # Should NOT be able to send torque
    self._set_prev_torque(0)
    for _ in range(6):
      self._rx(self._torque_meas_msg(0))
    self.assertFalse(self._tx(self._torque_cmd_msg(10, steer_req=1)))

    # Clear steering_disengage
    self.safety.set_steering_disengage(False)

    # ALKA should work again
    self.assertTrue(self.safety.get_lat_control_allowed())


class TestALKAToyotaDSU(TestALKABase):
  """Test ALKA functionality for Toyota with ACC_MAIN_DSU flag.

  When ACC_MAIN_DSU flag is set, Toyota uses DSU_CRUISE (0x365) instead of
  PCM_CRUISE_2 (0x1D3) for ACC main detection. This is used for Lexus IS, GS_F, RC
  and other DSU-based vehicles.
  """

  EPS_SCALE = 73

  def setUp(self):
    self.packer = CANPackerSafety("toyota_nodsu_pt_generated")
    self.safety = libsafety_py.libsafety
    # Set ACC_MAIN_DSU flag
    param = self.EPS_SCALE | ToyotaSafetyFlags.UNSUPPORTED_DSU
    self.safety.set_safety_hooks(CarParams.SafetyModel.toyota, param)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    # Reset ALKA state
    self.safety.set_lkas_on(False)

  def _speed_msg(self, speed):
    values = {("WHEEL_SPEED_%s" % n): speed * 3.6 for n in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_safety("WHEEL_SPEEDS", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEER_TORQUE_CMD": torque, "STEER_REQUEST": steer_req}
    return self.packer.make_can_msg_safety("STEERING_LKA", 0, values)

  def _dsu_cruise_msg(self, main_on):
    """Create DSU_CRUISE message (0x365, bit 0 = MAIN_ON)."""
    dat = bytearray(7)
    if main_on:
      dat[0] = 0x01  # bit 0
    # Calculate checksum
    s = 7  # length
    s += 0x365 & 0xFF
    s += (0x365 >> 8) & 0xFF
    for i in range(6):
      s += dat[i]
    dat[6] = s & 0xFF
    return libsafety_py.make_CANPacket(0x365, 0, bytes(dat))

  def _pcm_cruise_2_msg(self, main_on):
    """Create PCM_CRUISE_2 message (0x1D3, bit 15 = MAIN_ON)."""
    dat = bytearray(8)
    if main_on:
      dat[1] = 0x80  # bit 15
    s = 8
    s += 0x1D3 & 0xFF
    s += (0x1D3 >> 8) & 0xFF
    for i in range(7):
      s += dat[i]
    dat[7] = s & 0xFF
    return libsafety_py.make_CANPacket(0x1D3, 0, bytes(dat))

  def test_acc_main_dsu_flag_uses_0x365(self):
    """With ACC_MAIN_DSU flag, lkas_on should track DSU_CRUISE (0x365)."""
    # Initially off
    self._rx(self._dsu_cruise_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())

    # Turn on via DSU_CRUISE
    self._rx(self._dsu_cruise_msg(1))
    self.assertTrue(self.safety.get_lkas_on())
    self.assertTrue(self.safety.get_acc_main_on())

    # Turn off
    self._rx(self._dsu_cruise_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())

  def test_acc_main_dsu_ignores_0x1d3(self):
    """With ACC_MAIN_DSU flag, PCM_CRUISE_2 (0x1D3) should be ignored."""
    # Send PCM_CRUISE_2 with main on - should be ignored
    self._rx(self._pcm_cruise_2_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Only DSU_CRUISE should work
    self._rx(self._dsu_cruise_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

  def test_acc_main_dsu_alka_works(self):
    """Verify ALKA works with ACC_MAIN_DSU flag."""
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self.safety.set_controls_allowed(False)

    # Set vehicle moving and ACC main on via DSU_CRUISE
    self._set_vehicle_moving(True)
    self._rx(self._dsu_cruise_msg(1))

    # Should have lat control via ALKA
    self.assertTrue(self.safety.get_lat_control_allowed())

    # Turn off DSU_CRUISE - should lose lat control
    self._rx(self._dsu_cruise_msg(0))
    self.assertFalse(self.safety.get_lat_control_allowed())


class TestALKAToyotaDefaultPCM(TestALKABase):
  """Test Toyota default behavior (PCM_CRUISE_2) doesn't use DSU_CRUISE."""

  EPS_SCALE = 73

  def setUp(self):
    self.packer = CANPackerSafety("toyota_nodsu_pt_generated")
    self.safety = libsafety_py.libsafety
    # Default - no UNSUPPORTED_DSU flag
    self.safety.set_safety_hooks(CarParams.SafetyModel.toyota, self.EPS_SCALE)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    # Reset ALKA state
    self.safety.set_lkas_on(False)

  def _speed_msg(self, speed):
    values = {("WHEEL_SPEED_%s" % n): speed * 3.6 for n in ["FR", "FL", "RR", "RL"]}
    return self.packer.make_can_msg_safety("WHEEL_SPEEDS", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"STEER_TORQUE_CMD": torque, "STEER_REQUEST": steer_req}
    return self.packer.make_can_msg_safety("STEERING_LKA", 0, values)

  def _dsu_cruise_msg(self, main_on):
    """Create DSU_CRUISE message (0x365, bit 0 = MAIN_ON)."""
    dat = bytearray(7)
    if main_on:
      dat[0] = 0x01
    s = 7
    s += 0x365 & 0xFF
    s += (0x365 >> 8) & 0xFF
    for i in range(6):
      s += dat[i]
    dat[6] = s & 0xFF
    return libsafety_py.make_CANPacket(0x365, 0, bytes(dat))

  def _pcm_cruise_2_msg(self, main_on):
    """Create PCM_CRUISE_2 message (0x1D3, bit 15 = MAIN_ON)."""
    dat = bytearray(8)
    if main_on:
      dat[1] = 0x80
    s = 8
    s += 0x1D3 & 0xFF
    s += (0x1D3 >> 8) & 0xFF
    for i in range(7):
      s += dat[i]
    dat[7] = s & 0xFF
    return libsafety_py.make_CANPacket(0x1D3, 0, bytes(dat))

  def test_default_uses_0x1d3(self):
    """Without ACC_MAIN_DSU flag, lkas_on should track PCM_CRUISE_2 (0x1D3)."""
    # Initially off
    self._rx(self._pcm_cruise_2_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

    # Turn on via PCM_CRUISE_2
    self._rx(self._pcm_cruise_2_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Turn off
    self._rx(self._pcm_cruise_2_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

  def test_default_ignores_0x365(self):
    """Without ACC_MAIN_DSU flag, DSU_CRUISE (0x365) should be ignored."""
    # Send DSU_CRUISE with main on - should be ignored
    self._rx(self._dsu_cruise_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Only PCM_CRUISE_2 should work
    self._rx(self._pcm_cruise_2_msg(1))
    self.assertTrue(self.safety.get_lkas_on())


class TestALKAHyundai(TestALKABase):
  """Test ALKA functionality for Hyundai (dual: LKAS button toggle + ACC main state)."""

  def setUp(self):
    self.packer = CANPackerSafety("hyundai_kia_generic")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hyundai, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"WHL_SPD_FL": speed, "WHL_SPD_FR": speed, "WHL_SPD_RL": speed, "WHL_SPD_RR": speed}
    return self.packer.make_can_msg_safety("WHL_SPD11", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _torque_cmd_msg(self, torque, steer_req=1):
    values = {"CR_Lkas_StrToqReq": torque, "CF_Lkas_ActToi": steer_req}
    return self.packer.make_can_msg_safety("LKAS11", 0, values)

  def _lkas_button_msg(self, pressed):
    """Create LKAS button message (0x391 = BCM_PO_11)."""
    values = {"LDA_BTN": pressed}
    return self.packer.make_can_msg_safety("BCM_PO_11", 0, values)

  def _acc_main_msg(self, main_on):
    """Create ACC main message (SCC11 0x420, bit 0 = MainMode_ACC)."""
    values = {"MainMode_ACC": main_on}
    return self.packer.make_can_msg_safety("SCC11", 0, values)

  def test_alka_allowed_for_hyundai(self):
    """Verify alka_allowed flag is set in Hyundai safety mode init."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_button_toggle(self):
    """Verify LKAS button (BCM_PO_11) toggles lkas_on on rising edge."""
    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # Press button (rising edge) - should turn on
    self._rx(self._lkas_button_msg(0))  # release first
    self._rx(self._lkas_button_msg(1))  # press
    self.assertTrue(self.safety.get_lkas_on())

    # Hold button - should stay on
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Release button - should stay on
    self._rx(self._lkas_button_msg(0))
    self.assertTrue(self.safety.get_lkas_on())

    # Press again (rising edge) - should turn off
    self._rx(self._lkas_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Release - should stay off
    self._rx(self._lkas_button_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_acc_main_no_enable_on_rising_edge(self):
    """lkas_on should NOT enable on ACC main rising edge (SCC11 0x420)."""
    self._reset_safety_hooks()

    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # ACC main off -> on (rising edge) should NOT enable lkas_on
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())  # stays off

    # ACC main stays on - lkas_on should stay off
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_acc_main_off_disables_lkas(self):
    """ACC main OFF should disable lkas_on (master switch behavior)."""
    self._reset_safety_hooks()

    # First set ACC main on (doesn't enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Enable via button
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # ACC main off (falling edge) - lkas_on should turn off (main is master switch)
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_dual_tracking_button_can_disable(self):
    """Button should be able to disable ALKA even if ACC main is on."""
    self._reset_safety_hooks()

    # ACC main on (does not enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Button press should toggle on
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Button press again should toggle off
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKAHyundaiCameraSCC(TestALKABase):
  """Test ALKA functionality for Hyundai with camera SCC (ACC main on bus 2)."""

  HYUNDAI_PARAM_CAMERA_SCC = 8

  def setUp(self):
    self.packer = CANPackerSafety("hyundai_kia_generic")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hyundai, self.HYUNDAI_PARAM_CAMERA_SCC)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"WHL_SPD_FL": speed, "WHL_SPD_FR": speed, "WHL_SPD_RL": speed, "WHL_SPD_RR": speed}
    return self.packer.make_can_msg_safety("WHL_SPD11", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _acc_main_msg(self, main_on, bus):
    """Create ACC main message (SCC11 0x420, bit 0 = MainMode_ACC)."""
    dat = bytearray(8)
    if main_on:
      dat[0] = 0x01  # bit 0
    return libsafety_py.make_CANPacket(0x420, bus, bytes(dat))

  def _lkas_button_msg(self, pressed):
    values = {"LDA_BTN": pressed}
    return self.packer.make_can_msg_safety("BCM_PO_11", 0, values)

  def test_alka_camera_scc_uses_bus_2(self):
    """With camera_scc, ACC main on bus 2 acts as master disable switch."""
    # Bus 0 should be ignored for ACC main
    self._rx(self._acc_main_msg(1, bus=0))
    self.assertFalse(self.safety.get_lkas_on())

    # Bus 2 ACC main on (doesn't enable lkas_on - rising edge no longer enables)
    self._rx(self._acc_main_msg(0, bus=2))
    self._rx(self._acc_main_msg(1, bus=2))
    self.assertFalse(self.safety.get_lkas_on())

    # Enable via LKAS button
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # ACC main off on bus 2 disables ALKA (master switch behavior)
    self._rx(self._acc_main_msg(0, bus=2))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKAHonda(TestALKABase):
  """Test ALKA functionality for Honda."""

  def setUp(self):
    self.packer = CANPackerSafety("honda_civic_touring_2016_can_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hondaNidec, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"XMISSION_SPEED": speed * 3.6}
    return self.packer.make_can_msg_safety("ENGINE_DATA", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _main_on_msg(self, main_on):
    values = {"MAIN_ON": main_on}
    return self.packer.make_can_msg_safety("SCM_FEEDBACK", 0, values)

  def test_alka_allowed_for_honda(self):
    """Honda should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """lkas_on should follow acc_main_on for Honda."""
    # Turn on
    self._rx(self._main_on_msg(1))
    self.assertTrue(self.safety.get_lkas_on())
    self.assertTrue(self.safety.get_acc_main_on())

    # Turn off
    self._rx(self._main_on_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())


class TestALKAVolkswagenMQB(TestALKABase):
  """Test ALKA functionality for Volkswagen MQB."""

  def setUp(self):
    self.packer = CANPackerSafety("vw_mqb")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.volkswagen, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"ESP_VL_Radgeschw_02": speed * 3.6}
    return self.packer.make_can_msg_safety("ESP_19", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _tsk_status_msg(self, status):
    """TSK_Status: 2=standby/main on, 3=enabled, 0=off."""
    values = {"TSK_Status": status}
    return self.packer.make_can_msg_safety("TSK_06", 0, values)

  def test_alka_allowed_for_vw(self):
    """VW should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """lkas_on should follow acc_main_on for VW."""
    # Off
    self._rx(self._tsk_status_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

    # Standby (main on)
    self._rx(self._tsk_status_msg(2))
    self.assertTrue(self.safety.get_lkas_on())

    # Enabled
    self._rx(self._tsk_status_msg(3))
    self.assertTrue(self.safety.get_lkas_on())


class TestALKAHondaBosch(TestALKABase):
  """Test ALKA functionality for Honda Bosch."""

  def setUp(self):
    self.packer = CANPackerSafety("honda_civic_hatchback_ex_2017_can_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hondaBosch, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"XMISSION_SPEED": speed * 3.6}
    return self.packer.make_can_msg_safety("ENGINE_DATA", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _main_on_msg(self, main_on):
    """Honda Bosch uses 0x326 for ACC main (MAIN_ON at bit 28)."""
    values = {"MAIN_ON": main_on}
    return self.packer.make_can_msg_safety("SCM_FEEDBACK", 0, values)

  def test_alka_allowed_for_honda_bosch(self):
    """Honda Bosch should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())


class TestALKAHyundaiCanfd(TestALKABase):
  """Test ALKA functionality for Hyundai CAN-FD (dual: LKAS button toggle + ACC main state)."""

  def setUp(self):
    self.packer = CANPackerSafety("hyundai_canfd_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hyundaiCanfd, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"WHEEL_SPEED_1": speed, "WHEEL_SPEED_2": speed, "WHEEL_SPEED_3": speed, "WHEEL_SPEED_4": speed}
    return self.packer.make_can_msg_safety("WHEEL_SPEEDS", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _lkas_button_msg(self, pressed):
    """Create LKAS button message for CAN-FD (LDA_BTN at bit 23 in CRUISE_BUTTONS 0x1CF)."""
    values = {"LDA_BTN": pressed}
    return self.packer.make_can_msg_safety("CRUISE_BUTTONS", 0, values)

  def _acc_main_msg(self, main_on):
    """Create ACC main message for CAN-FD (SCC_CONTROL 0x1A0, bit 66 = MainMode_ACC)."""
    values = {"MainMode_ACC": main_on, "ACCMode": 0}
    return self.packer.make_can_msg_safety("SCC_CONTROL", 0, values)

  def test_alka_allowed_for_hyundai_canfd(self):
    """Hyundai CAN-FD should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_button_toggle(self):
    """lkas_on should toggle on LKAS button press (rising edge)."""
    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # Press button (rising edge) - should turn on
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Press again (rising edge) - should turn off
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_acc_main_no_enable_on_rising_edge(self):
    """lkas_on should NOT enable on ACC main rising edge (SCC_CONTROL 0x1A0)."""
    self._reset_safety_hooks()

    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # ACC main off -> on (rising edge) should NOT enable lkas_on
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())  # stays off

    # ACC main stays on - lkas_on should stay off
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_acc_main_off_disables_lkas(self):
    """ACC main OFF should disable lkas_on (master switch behavior)."""
    self._reset_safety_hooks()

    # First set ACC main on (doesn't enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Enable via button
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # ACC main off (falling edge) - lkas_on should turn off (main is master switch)
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_dual_tracking_button_can_disable(self):
    """Button should be able to toggle ALKA when ACC main is on."""
    self._reset_safety_hooks()

    # ACC main on (does not enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Button press should toggle on
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Button press again should toggle off
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKAHyundaiLegacy(TestALKABase):
  """Test ALKA functionality for Hyundai Legacy (dual: LKAS button toggle + ACC main state)."""

  def setUp(self):
    self.packer = CANPackerSafety("hyundai_kia_generic")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.hyundaiLegacy, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"WHL_SPD_FL": speed, "WHL_SPD_FR": speed, "WHL_SPD_RL": speed, "WHL_SPD_RR": speed}
    return self.packer.make_can_msg_safety("WHL_SPD11", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _lkas_button_msg(self, pressed):
    values = {"LDA_BTN": pressed}
    return self.packer.make_can_msg_safety("BCM_PO_11", 0, values)

  def _acc_main_msg(self, main_on):
    """Create ACC main message (SCC11 0x420, bit 0 = MainMode_ACC)."""
    values = {"MainMode_ACC": main_on}
    return self.packer.make_can_msg_safety("SCC11", 0, values)

  def test_alka_allowed_for_hyundai_legacy(self):
    """Hyundai Legacy should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_button_toggle(self):
    """lkas_on should toggle on LKAS button press (rising edge)."""
    self.assertFalse(self.safety.get_lkas_on())
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

  def test_alka_acc_main_no_enable_on_rising_edge(self):
    """lkas_on should NOT enable on ACC main rising edge (SCC11 0x420)."""
    self._reset_safety_hooks()

    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # ACC main off -> on (rising edge) should NOT enable lkas_on
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())  # stays off

  def test_alka_acc_main_off_disables_lkas(self):
    """ACC main OFF should disable lkas_on (master switch behavior)."""
    self._reset_safety_hooks()

    # First set ACC main on (doesn't enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Enable via button
    self._rx(self._lkas_button_msg(0))
    self._rx(self._lkas_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # ACC main off (falling edge) - lkas_on should turn off (main is master switch)
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKAVolkswagenPQ(TestALKABase):
  """Test ALKA functionality for Volkswagen PQ."""

  def setUp(self):
    self.packer = CANPackerSafety("vw_pq")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.volkswagenPq, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"Geschwindigkeit_neu__Bremse_1_": speed * 3.6}
    return self.packer.make_can_msg_safety("Bremse_1", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def test_alka_allowed_for_vw_pq(self):
    """VW PQ should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())


class TestALKASubaru(TestALKABase):
  """Test ALKA functionality for Subaru."""

  def setUp(self):
    self.packer = CANPackerSafety("subaru_global_2017_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.subaru, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"FR": speed, "FL": speed, "RR": speed, "RL": speed}
    return self.packer.make_can_msg_safety("Wheel_Speeds", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _cruise_msg(self, main_on):
    """CruiseControl message for ACC main."""
    values = {"Cruise_On": main_on}
    return self.packer.make_can_msg_safety("CruiseControl", 0, values)

  def test_alka_allowed_for_subaru(self):
    """Subaru should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """lkas_on should follow acc_main_on for Subaru."""
    # Turn on
    self._rx(self._cruise_msg(1))
    self.assertTrue(self.safety.get_lkas_on())
    self.assertTrue(self.safety.get_acc_main_on())

    # Turn off
    self._rx(self._cruise_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())


class TestALKASubaruPreglobal(TestALKABase):
  """Test ALKA functionality for Subaru Preglobal."""

  def setUp(self):
    self.packer = CANPackerSafety("subaru_outback_2015_generated")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.subaruPreglobal, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"FR": speed, "FL": speed, "RR": speed, "RL": speed}
    return self.packer.make_can_msg_safety("Wheel_Speeds", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _cruise_msg(self, main_on):
    """CruiseControl message for ACC main."""
    values = {"Cruise_On": main_on}
    return self.packer.make_can_msg_safety("CruiseControl", 0, values)

  def test_alka_allowed_for_subaru_preglobal(self):
    """Subaru Preglobal should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """lkas_on should follow acc_main_on for Subaru Preglobal."""
    self._rx(self._cruise_msg(1))
    self.assertTrue(self.safety.get_lkas_on())
    self._rx(self._cruise_msg(0))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKAMazda(TestALKABase):
  """Test ALKA functionality for Mazda."""

  def setUp(self):
    self.packer = CANPackerSafety("mazda_2017")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.mazda, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"SPEED": speed * 3.6}
    return self.packer.make_can_msg_safety("ENGINE_DATA", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def test_alka_allowed_for_mazda(self):
    """Mazda should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())


class TestALKANissan(TestALKABase):
  """Test ALKA functionality for Nissan Leaf (uses 0x239 on bus 0)."""

  def setUp(self):
    self.packer = CANPackerSafety("nissan_leaf_2018_generated")
    self.safety = libsafety_py.libsafety
    # Default param (no alt_eps flag)
    self.safety.set_safety_hooks(CarParams.SafetyModel.nissan, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"WHEEL_SPEED_RR": speed * 3.6, "WHEEL_SPEED_RL": speed * 3.6}
    return self.packer.make_can_msg_safety("WHEEL_SPEEDS_REAR", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      self._rx(self._speed_msg(speed))

  def _acc_main_msg_leaf(self, main_on):
    """Create Leaf ACC main message (CRUISE_THROTTLE 0x239, bit 17)."""
    dat = bytearray(8)
    if main_on:
      dat[2] = 0x02  # bit 17 = byte 2, bit 1
    return libsafety_py.make_CANPacket(0x239, 0, bytes(dat))

  def test_alka_allowed_for_nissan(self):
    """Nissan should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_lkas_on_from_acc_main(self):
    """Leaf lkas_on should track ACC main from CRUISE_THROTTLE (0x239) on bus 0."""
    # Initially off
    self._rx(self._acc_main_msg_leaf(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())

    # Turn on
    self._rx(self._acc_main_msg_leaf(1))
    self.assertTrue(self.safety.get_lkas_on())
    self.assertTrue(self.safety.get_acc_main_on())

    # Turn off
    self._rx(self._acc_main_msg_leaf(0))
    self.assertFalse(self.safety.get_lkas_on())
    self.assertFalse(self.safety.get_acc_main_on())


class TestALKAFord(TestALKABase):
  """Test ALKA functionality for Ford (dual: TJA button toggle + ACC main state)."""

  def setUp(self):
    self.packer = CANPackerSafety("ford_lincoln_base_pt")
    self.safety = libsafety_py.libsafety
    self.safety.set_safety_hooks(CarParams.SafetyModel.ford, 0)
    self.safety.init_tests()
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)

  def _speed_msg(self, speed):
    values = {"VehYaw_W_Actl": 0, "VehSpd_D_Actl": 0}
    # Ford uses different speed signal
    return self.packer.make_can_msg_safety("Yaw_Data_FD1", 0, values)

  def _set_vehicle_moving(self, moving: bool):
    # Ford has complex speed handling; for ALKA test we just set via wheel speeds
    values = {"WhlFl_W_Meas": 10.0 if moving else 0.0,
              "WhlFr_W_Meas": 10.0 if moving else 0.0,
              "WhlRl_W_Meas": 10.0 if moving else 0.0,
              "WhlRr_W_Meas": 10.0 if moving else 0.0}
    for _ in range(6):
      self._rx(self.packer.make_can_msg_safety("WheelSpeed_CG1", 0, values))

  def _acc_main_msg(self, main_on):
    """EngBrakeData message for ACC main (CcStat)."""
    # CcStat: 0=Off, 1=Denied, 2=Standby_Denied, 3=Standby, 4=Active_Que_Assist, 5=Active
    # acc_main_on is true when state == 3 (Standby) or cruise_engaged (4, 5)
    values = {"CcStat_D_Actl": 3 if main_on else 0}
    return self.packer.make_can_msg_safety("EngBrakeData", 0, values)

  def _tja_button_msg(self, pressed):
    """Steering_Data_FD1 message for TJA button (bit 40 = TjaButtnOnOffPress)."""
    dat = bytearray(8)
    if pressed:
      dat[5] = 0x01  # bit 40 = byte 5, bit 0
    return libsafety_py.make_CANPacket(0x083, 0, bytes(dat))

  def _torque_cmd_msg(self, torque, steer_req=1):
    # Ford doesn't use torque commands in the same way, not needed for these tests
    raise NotImplementedError

  def test_alka_allowed_for_ford(self):
    """Ford should have alka_allowed set to true."""
    self.assertTrue(self.safety.get_alka_allowed())

  def test_alka_acc_main_no_enable_on_rising_edge(self):
    """lkas_on should NOT enable on ACC main rising edge."""
    self._reset_safety_hooks()

    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # ACC main off -> on (rising edge) should NOT enable lkas_on
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())  # stays off

  def test_alka_acc_main_off_disables_lkas(self):
    """ACC main OFF should disable lkas_on (master switch behavior)."""
    self._reset_safety_hooks()

    # First set ACC main on (doesn't enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # Enable via TJA button
    self._rx(self._tja_button_msg(0))
    self._rx(self._tja_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # ACC main off (falling edge) - lkas_on should turn off (main is master switch)
    self._rx(self._acc_main_msg(0))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_tja_button_toggle(self):
    """TJA button should toggle lkas_on on rising edge."""
    self._reset_safety_hooks()

    # Initially off
    self.assertFalse(self.safety.get_lkas_on())

    # Press TJA button (rising edge) - should turn on
    self._rx(self._tja_button_msg(0))
    self._rx(self._tja_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # Press again (rising edge) - should turn off
    self._rx(self._tja_button_msg(0))
    self._rx(self._tja_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

  def test_alka_dual_tracking_button_can_disable(self):
    """TJA button should be able to toggle ALKA when ACC main is on."""
    self._reset_safety_hooks()

    # ACC main on (does not enable lkas_on)
    self._rx(self._acc_main_msg(0))
    self._rx(self._acc_main_msg(1))
    self.assertFalse(self.safety.get_lkas_on())

    # TJA button press should toggle on
    self._rx(self._tja_button_msg(0))
    self._rx(self._tja_button_msg(1))
    self.assertTrue(self.safety.get_lkas_on())

    # TJA button press again should toggle off
    self._rx(self._tja_button_msg(0))
    self._rx(self._tja_button_msg(1))
    self.assertFalse(self.safety.get_lkas_on())


class TestALKADisabledBrands(unittest.TestCase):
  """Test that ALKA is disabled for non-priority brands."""

  TX_MSGS = []  # Required by test_tx_hook_on_wrong_safety_mode

  def test_alka_disabled_for_gm(self):
    """GM should have alka_allowed set to false."""
    safety = libsafety_py.libsafety
    safety.set_safety_hooks(CarParams.SafetyModel.gm, 1)  # GM_PARAM_HW_CAM
    safety.init_tests()
    self.assertFalse(safety.get_alka_allowed())

  def test_alka_disabled_for_tesla(self):
    """Tesla should have alka_allowed set to false."""
    safety = libsafety_py.libsafety
    safety.set_safety_hooks(CarParams.SafetyModel.tesla, 0)
    safety.init_tests()
    self.assertFalse(safety.get_alka_allowed())

  def test_alka_disabled_for_chrysler(self):
    """Chrysler should have alka_allowed set to false (needs special handling)."""
    safety = libsafety_py.libsafety
    safety.set_safety_hooks(CarParams.SafetyModel.chrysler, 0)
    safety.init_tests()
    self.assertFalse(safety.get_alka_allowed())


class TestALKALatControlAllowed(unittest.TestCase):
  """Test lat_control_allowed() logic directly."""

  TX_MSGS = []  # Required by test_tx_hook_on_wrong_safety_mode

  def setUp(self):
    self.safety = libsafety_py.libsafety
    # Use Toyota as a test platform (alka_allowed = true)
    self.safety.set_safety_hooks(CarParams.SafetyModel.toyota, 73)
    self.safety.init_tests()

  def _set_vehicle_moving(self, moving: bool):
    packer = CANPackerSafety("toyota_nodsu_pt_generated")
    speed = 10.0 if moving else 0.0
    for _ in range(6):
      msg = packer.make_can_msg_safety("WHEEL_SPEEDS", 0, {
        "WHEEL_SPEED_FR": speed * 3.6,
        "WHEEL_SPEED_FL": speed * 3.6,
        "WHEEL_SPEED_RR": speed * 3.6,
        "WHEEL_SPEED_RL": speed * 3.6,
      })
      self.safety.safety_rx_hook(msg)

  def test_controls_allowed_overrides_alka(self):
    """controls_allowed should always grant lat control."""
    self.safety.set_controls_allowed(True)
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.DEFAULT)
    self.assertTrue(self.safety.get_lat_control_allowed())

  def test_alka_all_conditions_required(self):
    """All ALKA conditions must be met for lat control when controls_allowed is false."""
    self.safety.set_controls_allowed(False)

    # Test all combinations of conditions (including steering_disengage)
    for alka_flag in [False, True]:
      for lkas_on in [False, True]:
        for vehicle_moving in [False, True]:
          for steering_disengage in [False, True]:
            exp = ALTERNATIVE_EXPERIENCE.ALKA if alka_flag else ALTERNATIVE_EXPERIENCE.DEFAULT
            self.safety.set_alternative_experience(exp)
            self.safety.set_lkas_on(lkas_on)
            self._set_vehicle_moving(vehicle_moving)
            self.safety.set_steering_disengage(steering_disengage)

            # ALKA allows lat control only when ALL conditions are met
            # Note: alka_allowed is already true for Toyota
            expected = alka_flag and lkas_on and vehicle_moving and not steering_disengage
            self.assertEqual(expected, self.safety.get_lat_control_allowed(),
                             f"alka_flag={alka_flag}, lkas_on={lkas_on}, vehicle_moving={vehicle_moving}, steering_disengage={steering_disengage}")

  def test_steering_disengage_disables_alka(self):
    """steering_disengage should disable ALKA even when all other conditions are met."""
    self.safety.set_controls_allowed(False)
    self.safety.set_alternative_experience(ALTERNATIVE_EXPERIENCE.ALKA)
    self.safety.set_lkas_on(True)
    self._set_vehicle_moving(True)

    # Without steering_disengage, ALKA should work
    self.safety.set_steering_disengage(False)
    self.assertTrue(self.safety.get_lat_control_allowed())

    # With steering_disengage, ALKA should be disabled
    self.safety.set_steering_disengage(True)
    self.assertFalse(self.safety.get_lat_control_allowed())

  def test_controls_allowed_not_affected_by_steering_disengage(self):
    """controls_allowed should grant lat control regardless of steering_disengage.

    Note: In the real system, steering_disengage would set controls_allowed = false
    via the rising edge detection in safety.h. This test verifies that
    lat_control_allowed() itself doesn't block controls_allowed due to steering_disengage.
    """
    self.safety.set_controls_allowed(True)
    self.safety.set_steering_disengage(True)
    # controls_allowed still grants lat control (the real system resets controls_allowed separately)
    self.assertTrue(self.safety.get_lat_control_allowed())


if __name__ == "__main__":
  unittest.main()
