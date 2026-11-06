//! Alert engine: a pure-Rust port of `flipper/alerts.c` (rules, sustain, hysteresis, cooldown).
//! Signals (LED/vibro) are dropped: here a rule is simply enabled or not. `link_lost` does not
//! apply on the PC side.

use crate::protocol::Telemetry;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Rule {
    CpuTemp,
    GpuTemp,
    CpuLoad,
    GpuLoad,
    RamLoad,
    VramLoad,
    DiskLoad,
    BatteryLow,
}

pub const RULES: [Rule; 8] = [
    Rule::CpuTemp,
    Rule::GpuTemp,
    Rule::CpuLoad,
    Rule::GpuLoad,
    Rule::RamLoad,
    Rule::VramLoad,
    Rule::DiskLoad,
    Rule::BatteryLow,
];

impl Rule {
    /// Same keys as `flipper/settings.c`.
    pub fn key(self) -> &'static str {
        match self {
            Rule::CpuTemp => "cpu_temp",
            Rule::GpuTemp => "gpu_temp",
            Rule::CpuLoad => "cpu_load",
            Rule::GpuLoad => "gpu_load",
            Rule::RamLoad => "ram_load",
            Rule::VramLoad => "vram_load",
            Rule::DiskLoad => "disk_load",
            Rule::BatteryLow => "battery_low",
        }
    }

    pub fn unit(self) -> &'static str {
        match self {
            Rule::CpuTemp | Rule::GpuTemp => "C",
            _ => "%",
        }
    }

    fn lower_is_worse(self) -> bool {
        self == Rule::BatteryLow
    }

    fn index(self) -> usize {
        self as usize
    }

    /// The monitored value, or `None` when the field is not valid in this frame.
    fn value(self, t: &Telemetry) -> Option<u8> {
        match self {
            Rule::CpuTemp => t.cpu_temp_valid.then_some(t.cpu_temp),
            Rule::GpuTemp => (t.gpu_present && t.gpu_temp_valid).then_some(t.gpu_temp),
            Rule::CpuLoad => Some(t.cpu_load),
            Rule::GpuLoad => t.gpu_present.then_some(t.gpu_load),
            Rule::RamLoad => Some(t.ram_load),
            Rule::VramLoad => t.gpu_present.then_some(t.vram_load),
            Rule::DiskLoad => Some(t.disk_load),
            Rule::BatteryLow => (t.battery_present && t.on_battery).then_some(t.battery),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RuleConfig {
    pub enabled: bool,
    pub threshold: u8,
    /// Condition must hold this long before firing.
    pub sustain_s: u32,
    /// Clears when the value is this far back from the threshold.
    pub hysteresis: u8,
}

pub fn default_config(rule: Rule) -> RuleConfig {
    let c = RuleConfig { enabled: true, threshold: 90, sustain_s: 0, hysteresis: 5 };
    match rule {
        Rule::CpuTemp => c,
        Rule::GpuTemp => RuleConfig { threshold: 85, ..c },
        Rule::CpuLoad => RuleConfig { threshold: 95, sustain_s: 30, ..c },
        Rule::GpuLoad => RuleConfig { enabled: false, threshold: 98, sustain_s: 30, ..c },
        Rule::RamLoad => RuleConfig { sustain_s: 10, ..c },
        Rule::VramLoad => RuleConfig { threshold: 95, sustain_s: 10, ..c },
        Rule::DiskLoad => RuleConfig { threshold: 95, ..c },
        Rule::BatteryLow => RuleConfig { threshold: 20, ..c },
    }
}

#[derive(Debug, Clone, Copy, Default)]
struct RuleState {
    active: bool,
    fired_this_episode: bool,
    over_since_s: u32,
    last_fired_s: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AlertEvent {
    pub rule: Rule,
    pub value: u8,
    pub threshold: u8,
}

#[derive(Debug, Clone)]
pub struct AlertEngine {
    pub cfg: [RuleConfig; RULES.len()],
    st: [RuleState; RULES.len()],
    /// Repeat cooldown, default 300 s.
    pub cooldown_s: u32,
}

impl Default for AlertEngine {
    fn default() -> Self {
        AlertEngine { cfg: RULES.map(default_config), st: [RuleState::default(); RULES.len()], cooldown_s: 300 }
    }
}

impl AlertEngine {
    #[cfg(test)]
    pub fn is_active(&self, rule: Rule) -> bool {
        self.st[rule.index()].active
    }

    /// Evaluate all rules at `now_s` (monotonic seconds) and return the alerts that fire now.
    pub fn update(&mut self, now_s: u32, t: &Telemetry) -> Vec<AlertEvent> {
        let mut out = Vec::new();
        for rule in RULES {
            let c = self.cfg[rule.index()];
            let s = &mut self.st[rule.index()];
            let value = if c.enabled { rule.value(t) } else { None };
            let cond = value.is_some_and(|v| {
                let (v, thr, hyst) = (i32::from(v), i32::from(c.threshold), i32::from(c.hysteresis));
                match (rule.lower_is_worse(), s.active) {
                    (true, true) => v <= thr + hyst,
                    (true, false) => v <= thr,
                    (false, true) => v >= thr - hyst,
                    (false, false) => v >= thr,
                }
            });
            let Some(v) = value.filter(|_| cond) else {
                s.active = false;
                s.fired_this_episode = false;
                continue;
            };
            if !s.active {
                s.active = true;
                s.fired_this_episode = false;
                s.over_since_s = now_s;
            }
            if now_s.wrapping_sub(s.over_since_s) < c.sustain_s {
                continue;
            }
            if s.fired_this_episode && now_s.wrapping_sub(s.last_fired_s) < self.cooldown_s {
                continue;
            }
            s.fired_this_episode = true;
            s.last_fired_s = now_s;
            out.push(AlertEvent { rule, value: v, threshold: c.threshold });
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn calm() -> Telemetry {
        Telemetry { cpu_temp_valid: true, cpu_temp: 50, ..Default::default() }
    }

    #[test]
    fn defaults_and_keys() {
        let e = AlertEngine::default();
        assert_eq!(e.cfg[Rule::CpuTemp.index()].threshold, 90);
        assert_eq!(e.cfg[Rule::GpuTemp.index()].threshold, 85);
        assert!(!e.cfg[Rule::GpuLoad.index()].enabled);
        assert_eq!(e.cooldown_s, 300);
        assert_eq!(Rule::BatteryLow.key(), "battery_low");
        assert_eq!(Rule::VramLoad.key(), "vram_load");
    }

    #[test]
    fn fires_cooldown_hysteresis() {
        let mut e = AlertEngine::default();
        let mut t = calm();
        assert!(e.update(0, &t).is_empty());
        t.cpu_temp = 91;
        let ev = e.update(1, &t);
        assert_eq!(ev, vec![AlertEvent { rule: Rule::CpuTemp, value: 91, threshold: 90 }]);
        assert!(e.update(100, &t).is_empty());
        assert!(e.update(300, &t).is_empty());
        assert_eq!(e.update(301, &t).len(), 1);
        // Hysteresis: 86 keeps it active, 84 clears.
        t.cpu_temp = 86;
        e.update(302, &t);
        assert!(e.is_active(Rule::CpuTemp));
        t.cpu_temp = 84;
        e.update(303, &t);
        assert!(!e.is_active(Rule::CpuTemp));
        // A new episode fires immediately, ignoring the old cooldown.
        t.cpu_temp = 95;
        assert_eq!(e.update(304, &t).len(), 1);
    }

    #[test]
    fn invalid_field_never_fires() {
        let mut e = AlertEngine::default();
        let t = Telemetry { cpu_temp_valid: false, cpu_temp: 120, ..Default::default() };
        assert!(e.update(0, &t).is_empty());
        assert!(e.update(1, &t).is_empty());
    }

    #[test]
    fn sustain_delays_firing_and_resets() {
        let mut e = AlertEngine::default();
        let mut t = calm();
        t.ram_load = 95;
        for s in 0..10 {
            assert!(e.update(s, &t).is_empty(), "t={s}");
        }
        assert_eq!(e.update(10, &t).len(), 1);
        // Dropping below the threshold mid-way restarts the sustain timer.
        let mut e = AlertEngine::default();
        for s in 0..8 {
            assert!(e.update(s, &t).is_empty());
        }
        t.ram_load = 70;
        e.update(8, &t);
        t.ram_load = 95;
        for s in 9..19 {
            assert!(e.update(s, &t).is_empty(), "t={s}");
        }
        assert_eq!(e.update(19, &t).len(), 1);
    }

    #[test]
    fn disabled_rule_is_silent() {
        let mut e = AlertEngine::default();
        e.cfg[Rule::CpuTemp.index()].enabled = false;
        let mut t = calm();
        t.cpu_temp = 99;
        assert!(e.update(0, &t).is_empty());
        e.cfg[Rule::CpuTemp.index()].enabled = true;
        assert_eq!(e.update(1, &t).len(), 1);
    }

    #[test]
    fn battery_low_only_on_battery_and_lower_is_worse() {
        let mut e = AlertEngine::default();
        let mut t = calm();
        t.battery_present = true;
        t.battery = 15;
        t.on_battery = false;
        assert!(e.update(0, &t).is_empty(), "plugged in");
        t.on_battery = true;
        let ev = e.update(1, &t);
        assert_eq!(ev.len(), 1);
        assert_eq!(ev[0].rule, Rule::BatteryLow);
        t.battery = 24;
        e.update(2, &t);
        assert!(e.is_active(Rule::BatteryLow));
        t.battery = 26;
        e.update(3, &t);
        assert!(!e.is_active(Rule::BatteryLow));
    }

    #[test]
    fn gpu_rules_need_a_gpu() {
        let mut e = AlertEngine::default();
        let t = Telemetry { gpu_temp_valid: true, gpu_temp: 100, ..calm() };
        assert!(e.update(0, &t).is_empty());
        let t = Telemetry { gpu_present: true, ..t };
        assert_eq!(e.update(1, &t)[0].rule, Rule::GpuTemp);
    }
}
