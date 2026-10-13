// Filter config: packet IDs to ignore (denoise).
//
// Format (extension-superset of a plain list):
//   # comments
//   0x22
//   0x73
// or TOML-ish:
//   ignore = [0x22, 0x73]
//   ignore = ["0x22", "0x73"]
use std::collections::HashSet;
use std::fs;

#[derive(Default)]
pub struct Filter {
    ignore: HashSet<u8>,
}

impl Filter {
    pub fn from_file(path: &str) -> Result<Self, String> {
        let text = fs::read_to_string(path).map_err(|e| format!("{}: {}", path, e))?;
        let mut filter = Filter::default();
        let mut all_chars = String::new();
        for line in text.lines() {
            let line = line.split('#').next().unwrap_or("").trim();
            if line.is_empty() {
                continue;
            }
            // accumulate so `ignore = [` / items / `]` on multiple lines work
            all_chars.push_str(line);
            all_chars.push('\n');
        }
        if let Some(pos) = all_chars.find("ignore") {
            let rest = &all_chars[pos + "ignore".len()..];
            let rest = rest.trim_start().strip_prefix('=').unwrap_or(rest);
            for tok in rest.split(|c: char| c == ',' || c == '[' || c == ']' || c == '"' || c == '\n')
            {
                if let Some(id) = parse_id(tok) {
                    filter.ignore.insert(id);
                }
            }
        } else {
            for tok in all_chars.split_whitespace() {
                if let Some(id) = parse_id(tok) {
                    filter.ignore.insert(id);
                }
            }
        }
        Ok(filter)
    }

    pub fn ignore(&mut self, ids: &[u8]) {
        for id in ids {
            self.ignore.insert(*id);
        }
    }

    pub fn is_ignored(&self, id: u8) -> bool {
        self.ignore.contains(&id)
    }
}

fn parse_id(tok: &str) -> Option<u8> {
    let t = tok.trim();
    if t.is_empty() {
        return None;
    }
    let t = t.trim_start_matches("\"").trim_end_matches("\"");
    u8::from_str_radix(t.trim_start_matches("0x").trim_start_matches("0X"), 16).ok()
}
