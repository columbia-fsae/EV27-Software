"""Competition regulations relevant to the point-mass model: the electrical power cap."""
from __future__ import annotations

from dataclasses import dataclass, replace


@dataclass(eq=False)
class Regulations:
    power_limit: float  # watts

    def replace(self, **changes) -> "Regulations":
        return replace(self, **changes)


FSAE_EV = Regulations(power_limit=80e3)
FSAE_HYBRID = Regulations(power_limit=float("inf"))
