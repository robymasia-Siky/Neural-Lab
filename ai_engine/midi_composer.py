import random

SCALES = {
    'Fm': [53, 55, 56, 58, 60, 61, 63],
    'Am': [57, 59, 60, 62, 64, 65, 67],
    'Cm': [48, 50, 51, 53, 55, 56, 58],
    'Gm': [55, 57, 58, 60, 62, 63, 65],
    'Dm': [50, 52, 53, 55, 57, 58, 60]
}

class NeuralMidiComposer:
    def get_scale(self, key: str):
        return SCALES.get(key, SCALES['Fm'])

    def generate_rolling_bass(self, key: str = 'Fm', bars: int = 4):
        scale = self.get_scale(key)
        root = scale[0] - 12
        notes = []
        steps = bars * 16
        for step in range(steps):
            if step % 4 == 0:
                continue  # Lascia spazio al Kick sul quarto!
            pitch = root if (step % 4 != 3) else (root + 12 if random.random() > 0.6 else scale[4] - 12)
            # Velocity dinamica stile club (accento sul levare)
            vel = 112 if (step % 4 == 2) else random.randint(72, 92)
            start_beat = step * 0.25
            notes.append((pitch, round(start_beat, 3), 0.20, vel, 0))
        return notes

    def generate_hypnotic_arp(self, key: str = 'Fm', bars: int = 4):
        scale = self.get_scale(key)
        pattern_idx = [0, 2, 4, 2, 5, 4, 2, 1]
        notes = []
        steps = bars * 16
        for step in range(steps):
            if random.random() < 0.15:
                continue
            degree = pattern_idx[step % len(pattern_idx)]
            octave_shift = 12 if (step % 8 in [3, 7]) else 0
            pitch = scale[degree] + octave_shift
            # Crescendo di velocity lungo la battuta
            vel = min(124, 65 + (step % 16) * 3 + random.randint(-6, 8))
            start_beat = step * 0.25
            duration = 0.18 if (step % 2 == 0) else 0.30
            notes.append((pitch, round(start_beat, 3), duration, vel, 0))
        return notes

    def generate_filter_automation(self, bars: int = 4):
        # Curva di apertura filtro (0.25 -> 0.85) su 64 step
        points = []
        steps = bars * 16
        for s in range(steps):
            progress = s / float(max(1, steps - 1))
            val = round(0.25 + 0.60 * (progress ** 1.4), 3)
            points.append((round(s * 0.25, 3), val))
        return points
