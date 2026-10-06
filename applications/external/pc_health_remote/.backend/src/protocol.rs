//! Wire protocol v1 (see docs/PROTOCOL.md). All integers little-endian, fixed-size packed frames.

pub const MAGIC: [u8; 2] = [0x50, 0x48]; // "PH"
pub const VERSION: u8 = 1;
pub const HEADER_LEN: usize = 5;

pub const TYPE_TELEMETRY: u8 = 0x01;
pub const TYPE_HELLO: u8 = 0x81;

pub const TELEMETRY_LEN: usize = 48;
pub const HELLO_LEN: usize = 12;

/// Size of the `top_cpu_name` field.
pub const TOP_CPU_NAME_LEN: usize = 12;
/// Size of the `top_ram_name` field.
pub const TOP_RAM_NAME_LEN: usize = 8;

pub const FLAG_ON_BATTERY: u8 = 1 << 0;
pub const FLAG_CHARGING: u8 = 1 << 1;
pub const FLAG_CPU_TEMP_VALID: u8 = 1 << 2;
pub const FLAG_GPU_PRESENT: u8 = 1 << 3;
pub const FLAG_GPU_TEMP_VALID: u8 = 1 << 4;
pub const FLAG_BATTERY_PRESENT: u8 = 1 << 5;
pub const FLAG_FAN_VALID: u8 = 1 << 6;

/// CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0.
pub fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &b in data {
        crc ^= u16::from(b) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 { (crc << 1) ^ 0x1021 } else { crc << 1 };
        }
    }
    crc
}

/// PC -> Flipper telemetry sample (frame type 0x01).
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Telemetry {
    pub on_battery: bool,
    pub charging: bool,
    pub cpu_temp_valid: bool,
    pub gpu_present: bool,
    pub gpu_temp_valid: bool,
    pub battery_present: bool,
    pub fan_valid: bool,
    pub cpu_load: u8,
    pub cpu_temp: u8,
    pub gpu_load: u8,
    pub gpu_temp: u8,
    pub ram_load: u8,
    pub vram_load: u8,
    pub disk_load: u8,
    pub battery: u8,
    pub ram_total_dgb: u16,
    pub vram_total_dgb: u16,
    pub cpu_clock_mhz: u16,
    pub fan_rpm: u16,
    pub top_cpu_pct: u8,
    pub top_ram_pct: u8,
    pub top_cpu_name: [u8; TOP_CPU_NAME_LEN],
    pub top_ram_name: [u8; TOP_RAM_NAME_LEN],
    pub uptime_h: u16,
}

/// Copy `name` into a fixed NUL-padded ASCII field.
///
/// A trailing ".exe" is stripped, non-printable/non-ASCII bytes become '?', and at most
/// `N - 1` characters are kept so the field is always NUL-terminated (safe for C readers).
pub fn pack_name<const N: usize>(name: &str) -> [u8; N] {
    let trimmed = strip_exe(name);
    let mut out = [0u8; N];
    for (slot, ch) in out.iter_mut().take(N - 1).zip(trimmed.chars()) {
        *slot = if ch.is_ascii_graphic() || ch == ' ' { ch as u8 } else { b'?' };
    }
    out
}

/// Remove a case-insensitive ".exe" suffix.
pub fn strip_exe(name: &str) -> &str {
    let n = name.trim();
    if n.len() >= 4 && n.is_char_boundary(n.len() - 4) && n[n.len() - 4..].eq_ignore_ascii_case(".exe") {
        &n[..n.len() - 4]
    } else {
        n
    }
}

fn write_header(buf: &mut [u8], ftype: u8, seq: u8) {
    buf[0] = MAGIC[0];
    buf[1] = MAGIC[1];
    buf[2] = VERSION;
    buf[3] = ftype;
    buf[4] = seq;
}

fn seal(buf: &mut [u8]) {
    let n = buf.len() - 2;
    let crc = crc16(&buf[..n]);
    buf[n..].copy_from_slice(&crc.to_le_bytes());
}

impl Telemetry {
    pub fn flags(&self) -> u8 {
        let mut f = 0u8;
        let bits = [
            (self.on_battery, FLAG_ON_BATTERY),
            (self.charging, FLAG_CHARGING),
            (self.cpu_temp_valid, FLAG_CPU_TEMP_VALID),
            (self.gpu_present, FLAG_GPU_PRESENT),
            (self.gpu_temp_valid, FLAG_GPU_TEMP_VALID),
            (self.battery_present, FLAG_BATTERY_PRESENT),
            (self.fan_valid, FLAG_FAN_VALID),
        ];
        for (set, bit) in bits {
            if set {
                f |= bit;
            }
        }
        f
    }

    /// Serialise to a complete 48-byte frame.
    pub fn encode(&self, seq: u8) -> [u8; TELEMETRY_LEN] {
        let mut b = [0u8; TELEMETRY_LEN];
        write_header(&mut b, TYPE_TELEMETRY, seq);
        b[5] = self.flags();
        b[6] = self.cpu_load;
        b[7] = self.cpu_temp;
        b[8] = self.gpu_load;
        b[9] = self.gpu_temp;
        b[10] = self.ram_load;
        b[11] = self.vram_load;
        b[12] = self.disk_load;
        b[13] = self.battery;
        b[14..16].copy_from_slice(&self.ram_total_dgb.to_le_bytes());
        b[16..18].copy_from_slice(&self.vram_total_dgb.to_le_bytes());
        b[18..20].copy_from_slice(&self.cpu_clock_mhz.to_le_bytes());
        b[20..22].copy_from_slice(&self.fan_rpm.to_le_bytes());
        b[22] = self.top_cpu_pct;
        b[23] = self.top_ram_pct;
        b[24..36].copy_from_slice(&self.top_cpu_name);
        b[36..44].copy_from_slice(&self.top_ram_name);
        b[44..46].copy_from_slice(&self.uptime_h.to_le_bytes());
        seal(&mut b);
        b
    }

    /// Parse a complete, CRC-checked telemetry frame body (used by tests and tools).
    fn decode(b: &[u8]) -> Telemetry {
        let u16le = |o: usize| u16::from_le_bytes([b[o], b[o + 1]]);
        let f = b[5];
        let mut top_cpu_name = [0u8; TOP_CPU_NAME_LEN];
        top_cpu_name.copy_from_slice(&b[24..36]);
        let mut top_ram_name = [0u8; TOP_RAM_NAME_LEN];
        top_ram_name.copy_from_slice(&b[36..44]);
        Telemetry {
            on_battery: f & FLAG_ON_BATTERY != 0,
            charging: f & FLAG_CHARGING != 0,
            cpu_temp_valid: f & FLAG_CPU_TEMP_VALID != 0,
            gpu_present: f & FLAG_GPU_PRESENT != 0,
            gpu_temp_valid: f & FLAG_GPU_TEMP_VALID != 0,
            battery_present: f & FLAG_BATTERY_PRESENT != 0,
            fan_valid: f & FLAG_FAN_VALID != 0,
            cpu_load: b[6],
            cpu_temp: b[7],
            gpu_load: b[8],
            gpu_temp: b[9],
            ram_load: b[10],
            vram_load: b[11],
            disk_load: b[12],
            battery: b[13],
            ram_total_dgb: u16le(14),
            vram_total_dgb: u16le(16),
            cpu_clock_mhz: u16le(18),
            fan_rpm: u16le(20),
            top_cpu_pct: b[22],
            top_ram_pct: b[23],
            top_cpu_name,
            top_ram_name,
            uptime_h: u16le(44),
        }
    }
}

/// Flipper -> PC greeting (frame type 0x81).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Hello {
    pub seq: u8,
    /// major * 100 + minor
    pub app_version: u16,
    /// 0 = BLE, 1 = USB
    pub transport: u8,
    /// Requested telemetry interval, clamped to 1..=10 seconds.
    pub interval_s: u8,
}

impl Hello {
    /// Only the Flipper sends HELLO; the backend encodes it for tests.
    #[cfg(test)]
    pub fn encode(&self) -> [u8; HELLO_LEN] {
        let mut b = [0u8; HELLO_LEN];
        write_header(&mut b, TYPE_HELLO, self.seq);
        b[5..7].copy_from_slice(&self.app_version.to_le_bytes());
        b[7] = self.transport;
        b[8] = self.interval_s;
        seal(&mut b);
        b
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Frame {
    Hello(Hello),
    Telemetry(Telemetry),
}

fn frame_len(ftype: u8) -> Option<usize> {
    match ftype {
        TYPE_TELEMETRY => Some(TELEMETRY_LEN),
        TYPE_HELLO => Some(HELLO_LEN),
        _ => None,
    }
}

/// Parse exactly one frame from `b` (must be a complete frame). Returns `None` if the magic,
/// version, type, length or CRC is wrong.
pub fn parse_frame(b: &[u8]) -> Option<Frame> {
    if b.len() < HEADER_LEN || b[..2] != MAGIC || b[2] != VERSION {
        return None;
    }
    let want = frame_len(b[3])?;
    if b.len() != want {
        return None;
    }
    let crc = u16::from_le_bytes([b[want - 2], b[want - 1]]);
    if crc != crc16(&b[..want - 2]) {
        return None;
    }
    Some(match b[3] {
        TYPE_HELLO => Frame::Hello(Hello {
            seq: b[4],
            app_version: u16::from_le_bytes([b[5], b[6]]),
            transport: b[7],
            interval_s: b[8].clamp(1, 10),
        }),
        _ => Frame::Telemetry(Telemetry::decode(b)),
    })
}

/// Incremental stream parser: tolerates garbage, split frames and back-to-back frames, and
/// re-synchronises on the next valid "PH" header after any corruption.
#[derive(Default)]
pub struct FrameParser {
    buf: Vec<u8>,
}

impl FrameParser {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, data: &[u8]) -> Vec<Frame> {
        self.buf.extend_from_slice(data);
        let mut out = Vec::new();
        loop {
            // Align to the next magic.
            match self.buf.windows(2).position(|w| w == MAGIC) {
                Some(0) => {}
                Some(p) => {
                    self.buf.drain(..p);
                }
                None => {
                    // Keep a trailing 'P' that might be the first half of a magic.
                    let keep = usize::from(self.buf.last() == Some(&MAGIC[0]));
                    let cut = self.buf.len() - keep;
                    self.buf.drain(..cut);
                    break;
                }
            }
            if self.buf.len() < HEADER_LEN {
                break;
            }
            let want = match (self.buf[2] == VERSION).then(|| frame_len(self.buf[3])).flatten() {
                Some(n) => n,
                None => {
                    self.buf.remove(0);
                    continue;
                }
            };
            if self.buf.len() < want {
                break;
            }
            match parse_frame(&self.buf[..want]) {
                Some(f) => {
                    out.push(f);
                    self.buf.drain(..want);
                }
                None => {
                    // Bad CRC: this "PH" was likely a false start; skip one byte and rescan.
                    self.buf.remove(0);
                }
            }
        }
        // Never let a pathological stream grow the buffer.
        if self.buf.len() > 4 * TELEMETRY_LEN {
            self.buf.clear();
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> Telemetry {
        Telemetry {
            on_battery: true,
            charging: false,
            cpu_temp_valid: true,
            gpu_present: true,
            gpu_temp_valid: true,
            battery_present: true,
            fan_valid: true,
            cpu_load: 42,
            cpu_temp: 71,
            gpu_load: 88,
            gpu_temp: 66,
            ram_load: 55,
            vram_load: 33,
            disk_load: 77,
            battery: 64,
            ram_total_dgb: 160,
            vram_total_dgb: 60,
            cpu_clock_mhz: 4100,
            fan_rpm: 3210,
            top_cpu_pct: 17,
            top_ram_pct: 9,
            top_cpu_name: pack_name("chrome.exe"),
            top_ram_name: pack_name("Code.EXE"),
            uptime_h: 123,
        }
    }

    fn hello(seq: u8) -> Hello {
        Hello { seq, app_version: 100, transport: 1, interval_s: 2 }
    }

    #[test]
    fn crc_check_vector() {
        assert_eq!(crc16(b"123456789"), 0x29B1);
    }

    #[test]
    fn telemetry_is_48_bytes_with_expected_offsets() {
        let f = sample().encode(9);
        assert_eq!(f.len(), 48);
        assert_eq!(&f[0..2], b"PH");
        assert_eq!(f[2], 1);
        assert_eq!(f[3], 0x01);
        assert_eq!(f[4], 9);
        assert_eq!(f[5], 0b0111_1101);
        assert_eq!(&f[6..14], &[42, 71, 88, 66, 55, 33, 77, 64]);
        assert_eq!(u16::from_le_bytes([f[14], f[15]]), 160);
        assert_eq!(u16::from_le_bytes([f[16], f[17]]), 60);
        assert_eq!(u16::from_le_bytes([f[18], f[19]]), 4100);
        assert_eq!(u16::from_le_bytes([f[20], f[21]]), 3210);
        assert_eq!(f[22], 17);
        assert_eq!(f[23], 9);
        assert_eq!(&f[24..30], b"chrome");
        assert!(f[30..36].iter().all(|&b| b == 0));
        assert_eq!(&f[36..40], b"Code");
        assert!(f[40..44].iter().all(|&b| b == 0));
        assert_eq!(u16::from_le_bytes([f[44], f[45]]), 123);
        let crc = u16::from_le_bytes([f[46], f[47]]);
        assert_eq!(crc, crc16(&f[..46]));
    }

    #[test]
    fn flag_bits() {
        let mut t = Telemetry::default();
        assert_eq!(t.flags(), 0);
        t.on_battery = true;
        assert_eq!(t.flags(), 0x01);
        t.charging = true;
        t.cpu_temp_valid = true;
        t.gpu_present = true;
        t.gpu_temp_valid = true;
        t.battery_present = true;
        t.fan_valid = true;
        assert_eq!(t.flags(), 0x7F);
    }

    #[test]
    fn telemetry_round_trip() {
        let t = sample();
        let f = t.encode(200);
        assert_eq!(parse_frame(&f), Some(Frame::Telemetry(t)));
    }

    #[test]
    fn names_are_ascii_nul_padded_and_terminated() {
        let n: [u8; 12] = pack_name("A very long process name.exe");
        assert_eq!(&n[..11], b"A very long");
        assert_eq!(n[11], 0);
        let n: [u8; 8] = pack_name("héllo.exe");
        assert_eq!(&n[..5], b"h?llo");
        assert_eq!(&n[5..], &[0, 0, 0]);
        assert_eq!(strip_exe("x.Exe"), "x");
        assert_eq!(strip_exe(".exe"), "");
        assert_eq!(strip_exe("exe"), "exe");
    }

    #[test]
    fn hello_good() {
        let b = hello(3).encode();
        assert_eq!(b.len(), 12);
        assert_eq!(b[3], 0x81);
        assert_eq!(parse_frame(&b), Some(Frame::Hello(hello(3))));
    }

    #[test]
    fn hello_interval_is_clamped() {
        let mut h = hello(0);
        h.interval_s = 0;
        assert!(matches!(parse_frame(&h.encode()), Some(Frame::Hello(x)) if x.interval_s == 1));
        h.interval_s = 99;
        assert!(matches!(parse_frame(&h.encode()), Some(Frame::Hello(x)) if x.interval_s == 10));
    }

    #[test]
    fn hello_bad_crc_magic_version_length() {
        let good = hello(1).encode();
        let mut b = good;
        b[11] ^= 0xFF;
        assert_eq!(parse_frame(&b), None);
        let mut b = good;
        b[0] = b'X';
        assert_eq!(parse_frame(&b), None);
        let mut b = good;
        b[2] = 2;
        // fix CRC so only the version is wrong
        let crc = crc16(&b[..10]).to_le_bytes();
        b[10..].copy_from_slice(&crc);
        assert_eq!(parse_frame(&b), None);
        assert_eq!(parse_frame(&good[..11]), None);
    }

    #[test]
    fn stream_resync_after_garbage_and_bad_frames() {
        let mut bad = hello(1).encode();
        bad[7] ^= 1; // corrupt payload -> bad CRC
        let mut stream = vec![0x00, 0x50, 0x11, 0x50, 0x48, 0x77];
        stream.extend_from_slice(&bad);
        stream.extend_from_slice(&[0xAA; 5]);
        stream.extend_from_slice(&hello(2).encode());
        stream.extend_from_slice(&hello(3).encode());
        let mut p = FrameParser::new();
        let frames = p.push(&stream);
        assert_eq!(frames, vec![Frame::Hello(hello(2)), Frame::Hello(hello(3))]);
    }

    #[test]
    fn stream_handles_split_chunks_byte_by_byte() {
        let mut stream = Vec::new();
        stream.extend_from_slice(b"junkP");
        stream.extend_from_slice(&hello(7).encode());
        stream.extend_from_slice(&hello(8).encode());
        let mut p = FrameParser::new();
        let mut frames = Vec::new();
        for b in stream {
            frames.extend(p.push(&[b]));
        }
        assert_eq!(frames, vec![Frame::Hello(hello(7)), Frame::Hello(hello(8))]);
    }

    #[test]
    fn stream_drops_unknown_version_and_type() {
        let mut v2 = hello(1).encode();
        v2[2] = 2;
        let mut unk = hello(1).encode();
        unk[3] = 0x55;
        let mut stream = Vec::new();
        stream.extend_from_slice(&v2);
        stream.extend_from_slice(&unk);
        stream.extend_from_slice(&hello(4).encode());
        let mut p = FrameParser::new();
        assert_eq!(p.push(&stream), vec![Frame::Hello(hello(4))]);
    }

    #[test]
    fn stream_buffer_does_not_grow_without_bound() {
        let mut p = FrameParser::new();
        for _ in 0..1000 {
            p.push(&[0x50, 0x48, 1, 0x01, 0, 0, 0, 0]);
        }
        assert!(p.buf.len() <= 4 * TELEMETRY_LEN + 8);
    }
}
