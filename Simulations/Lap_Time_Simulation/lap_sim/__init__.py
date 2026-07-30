"""Point-mass lap time simulation library for FSAE EV vehicle design studies."""

from .battery import BatteryModel, StaticVoltageBattery
from .lap_simulator import LapSimulator
from .motors import DHX_K40, EMRAX_208, EMRAX_228, EMRAX_268, Motor
from .regulations import FSAE_EV, FSAE_HYBRID, Regulations
from .results import LapResult, LapStats
from .scoring import CompetitionScorer, ScoreBreakdown
from .sweeps import CompetitionEvaluator, PowerEnergySearch, grid_sweep
from .track import ACCEL_EVENT, ENDURANCE_EVENT, ICAHN_LOOP, SKIDPAD_EVENT, Track
from .vehicle import EV24, EV25, EV26A, Aero, Car, Drivetrain, HighVoltageSystem, Tires

__all__ = [
    "BatteryModel", "StaticVoltageBattery",
    "LapSimulator",
    "DHX_K40", "EMRAX_208", "EMRAX_228", "EMRAX_268", "Motor",
    "FSAE_EV", "FSAE_HYBRID", "Regulations",
    "LapResult", "LapStats",
    "CompetitionScorer", "ScoreBreakdown",
    "CompetitionEvaluator", "PowerEnergySearch", "grid_sweep",
    "ACCEL_EVENT", "ENDURANCE_EVENT", "ICAHN_LOOP", "SKIDPAD_EVENT", "Track",
    "EV24", "EV25", "EV26A", "Aero", "Car", "Drivetrain", "HighVoltageSystem", "Tires",
]
