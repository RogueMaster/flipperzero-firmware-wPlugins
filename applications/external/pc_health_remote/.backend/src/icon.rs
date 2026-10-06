//! Tray icon drawn at runtime: a coloured disc with a white heartbeat line.

/// Dot colour: connected / waiting / busy.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum IconState {
    Connected,
    Waiting,
    Pairing,
}

impl IconState {
    fn rgb(self) -> [f32; 3] {
        match self {
            IconState::Connected => [46.0, 160.0, 90.0],
            IconState::Waiting => [200.0, 130.0, 30.0],
            IconState::Pairing => [50.0, 120.0, 210.0],
        }
    }
}

const PULSE: [(f32, f32); 7] =
    [(0.10, 0.55), (0.30, 0.55), (0.38, 0.40), (0.46, 0.72), (0.56, 0.22), (0.64, 0.55), (0.90, 0.55)];

fn dist_to_segment(p: (f32, f32), a: (f32, f32), b: (f32, f32)) -> f32 {
    let (abx, aby) = (b.0 - a.0, b.1 - a.1);
    let len2 = abx * abx + aby * aby;
    let t = if len2 == 0.0 { 0.0 } else { (((p.0 - a.0) * abx + (p.1 - a.1) * aby) / len2).clamp(0.0, 1.0) };
    let (cx, cy) = (a.0 + t * abx, a.1 + t * aby);
    ((p.0 - cx).powi(2) + (p.1 - cy).powi(2)).sqrt()
}

/// RGBA pixels (row-major, `size * size * 4` bytes).
pub fn render(size: u32, state: IconState) -> Vec<u8> {
    let s = size as f32;
    let [r, g, b] = state.rgb();
    let radius = s / 2.0 - 0.5;
    let thick = (s * 0.085).max(1.2);
    let pts: Vec<(f32, f32)> = PULSE.iter().map(|&(x, y)| (x * s, y * s)).collect();
    let mut out = Vec::with_capacity((size * size * 4) as usize);
    for py in 0..size {
        for px in 0..size {
            let p = (px as f32 + 0.5, py as f32 + 0.5);
            let d_center = ((p.0 - s / 2.0).powi(2) + (p.1 - s / 2.0).powi(2)).sqrt();
            let disc = (radius - d_center + 0.5).clamp(0.0, 1.0);
            let line_d = pts.windows(2).map(|w| dist_to_segment(p, w[0], w[1])).fold(f32::MAX, f32::min);
            let line = (thick / 2.0 + 0.5 - line_d).clamp(0.0, 1.0) * disc;
            let mix = |c: f32| (c * (1.0 - line) + 255.0 * line).round() as u8;
            out.extend_from_slice(&[mix(r), mix(g), mix(b), (disc * 255.0).round() as u8]);
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn icon_has_expected_size_and_shape() {
        let px = render(32, IconState::Connected);
        assert_eq!(px.len(), 32 * 32 * 4);
        // Corner is transparent, centre of the disc is opaque.
        assert_eq!(px[3], 0);
        let centre = (16 * 32 + 16) * 4;
        assert_eq!(px[centre + 3], 255);
        // Some pixels of the white pulse line exist.
        assert!(px.chunks(4).any(|p| p[3] == 255 && p[0] > 240 && p[1] > 240 && p[2] > 240));
        assert_ne!(render(32, IconState::Waiting), px);
    }
}
