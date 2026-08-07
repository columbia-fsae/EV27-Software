import numpy as np

class CellModel():
    def __init__(self, r0, r1, r2, c1, c2, v_stats):
        self.r = [r0, r1, r2]
        self.c = [c1, c2]
        self.v_stats = np.array(v_stats)

    
        