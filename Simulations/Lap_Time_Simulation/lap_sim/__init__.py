"""Point-mass lap time simulation library for FSAE EV vehicle design studies."""

from .battery import Battery, StaticVoltageBattery
from .ecms import EcmsController, linear_soc_schedule, linear_temp_schedule
from .lap_simulator import LapSimulator
from .motors import DHX_K40, EMRAX_208, EMRAX_228, EMRAX_268, Motor
from .regulations import FSAE_EV, FSAE_HYBRID, Regulations
from .results import LapResult, LapStats
from .scoring import CompetitionScorer, ScoreBreakdown
from .sweeps import CompetitionEvaluator, PowerEnergySearch, grid_sweep
from .tire import Tire
from .track import ACCEL_EVENT, ENDURANCE_EVENT, ICAHN_LOOP, SKIDPAD_EVENT, Track
from .vehicle import EV24, EV25, EV26A, EV27, Aero, Car, Drivetrain, HighVoltageSystem

__all__ = [
    "Battery", "StaticVoltageBattery",
    "EcmsController", "linear_soc_schedule", "linear_temp_schedule",
    "LapSimulator",
    "DHX_K40", "EMRAX_208", "EMRAX_228", "EMRAX_268", "Motor",
    "FSAE_EV", "FSAE_HYBRID", "Regulations",
    "LapResult", "LapStats",
    "CompetitionScorer", "ScoreBreakdown",
    "CompetitionEvaluator", "PowerEnergySearch", "grid_sweep",
    "Tire",
    "ACCEL_EVENT", "ENDURANCE_EVENT", "ICAHN_LOOP", "SKIDPAD_EVENT", "Track",
    "EV24", "EV25", "EV26A", "EV27", "Aero", "Car", "Drivetrain", "HighVoltageSystem",
]
