//! Minimal JSON reader for project files and glTF documents.
//!
//! Supports the full JSON grammar plus `//` line comments so project files can
//! be annotated. No external crates, no allocation-heavy abstractions.

use std::collections::BTreeMap;

#[derive(Clone, Debug, PartialEq)]
pub enum Json {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<Json>),
    Object(BTreeMap<String, Json>),
}

impl Json {
    pub fn as_object(&self) -> Option<&BTreeMap<String, Json>> {
        match self {
            Json::Object(map) => Some(map),
            _ => None,
        }
    }

    pub fn as_array(&self) -> Option<&Vec<Json>> {
        match self {
            Json::Array(values) => Some(values),
            _ => None,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Json::String(text) => Some(text),
            _ => None,
        }
    }

    pub fn get(&self, key: &str) -> Option<&Json> {
        self.as_object().and_then(|map| map.get(key))
    }

    pub fn get_str(&self, key: &str) -> Option<&str> {
        self.get(key).and_then(Json::as_str)
    }

    pub fn get_bool(&self, key: &str) -> Option<bool> {
        match self.get(key) {
            Some(Json::Bool(value)) => Some(*value),
            _ => None,
        }
    }

    pub fn get_f64(&self, key: &str) -> Option<f64> {
        match self.get(key) {
            Some(Json::Number(value)) => Some(*value),
            _ => None,
        }
    }

    pub fn get_array(&self, key: &str) -> Option<&Vec<Json>> {
        self.get(key).and_then(Json::as_array)
    }

    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Json::Number(value) => Some(*value),
            _ => None,
        }
    }

    pub fn as_usize(&self) -> Option<usize> {
        match self {
            Json::Number(value) => {
                if *value >= 0.0 {
                    Some(*value as usize)
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Json::Bool(value) => Some(*value),
            _ => None,
        }
    }
}

struct Parser<'a> {
    bytes: &'a [u8],
    position: usize,
    depth: usize,
}

impl<'a> Parser<'a> {
    fn new(text: &'a str) -> Self {
        Self { bytes: text.as_bytes(), position: 0, depth: 0 }
    }

    fn error(&self, message: &str) -> String {
        format!("{} at byte {}", message, self.position)
    }

    fn peek(&self) -> Option<u8> {
        self.bytes.get(self.position).copied()
    }

    fn skip_trivia(&mut self) {
        loop {
            while matches!(self.peek(), Some(b' ') | Some(b'\t') | Some(b'\n') | Some(b'\r')) {
                self.position += 1;
            }
            if self.peek() == Some(b'/') && self.bytes.get(self.position + 1) == Some(&b'/') {
                while let Some(byte) = self.peek() {
                    self.position += 1;
                    if byte == b'\n' {
                        break;
                    }
                }
                continue;
            }
            break;
        }
    }

    fn parse_value(&mut self) -> Result<Json, String> {
        if self.depth > 128 {
            return Err(self.error("maximum nesting depth exceeded"));
        }
        self.skip_trivia();
        match self.peek() {
            Some(b'{') => self.parse_object(),
            Some(b'[') => self.parse_array(),
            Some(b'"') => Ok(Json::String(self.parse_string()?)),
            Some(b't') => {
                self.expect_literal("true")?;
                Ok(Json::Bool(true))
            }
            Some(b'f') => {
                self.expect_literal("false")?;
                Ok(Json::Bool(false))
            }
            Some(b'n') => {
                self.expect_literal("null")?;
                Ok(Json::Null)
            }
            Some(_) => self.parse_number(),
            None => Err(self.error("unexpected end of input")),
        }
    }

    fn expect_literal(&mut self, literal: &str) -> Result<(), String> {
        if self.bytes[self.position..].starts_with(literal.as_bytes()) {
            self.position += literal.len();
            Ok(())
        } else {
            Err(self.error("invalid literal"))
        }
    }

    fn parse_number(&mut self) -> Result<Json, String> {
        let start = self.position;
        while let Some(byte) = self.peek() {
            let is_number_char = matches!(byte, b'0'..=b'9' | b'-' | b'+' | b'.' | b'e' | b'E');
            if !is_number_char {
                break;
            }
            self.position += 1;
        }
        if start == self.position {
            return Err(self.error("invalid number"));
        }
        let text = std::str::from_utf8(&self.bytes[start..self.position]).map_err(|_| self.error("invalid utf-8"))?;
        text.parse::<f64>()
            .map(Json::Number)
            .map_err(|_| self.error("invalid number"))
    }

    fn parse_string(&mut self) -> Result<String, String> {
        if self.peek() != Some(b'"') {
            return Err(self.error("expected a string"));
        }
        self.position += 1;
        let mut output = String::new();
        loop {
            let byte = self.peek().ok_or_else(|| self.error("unterminated string"))?;
            self.position += 1;
            match byte {
                b'"' => return Ok(output),
                b'\\' => {
                    let escape = self.peek().ok_or_else(|| self.error("unterminated escape"))?;
                    self.position += 1;
                    match escape {
                        b'"' => output.push('"'),
                        b'\\' => output.push('\\'),
                        b'/' => output.push('/'),
                        b'b' => output.push('\u{0008}'),
                        b'f' => output.push('\u{000C}'),
                        b'n' => output.push('\n'),
                        b'r' => output.push('\r'),
                        b't' => output.push('\t'),
                        b'u' => {
                            let code = self.parse_hex4()?;
                            let character = char::from_u32(code)
                                .ok_or_else(|| self.error("invalid unicode escape"))?;
                            output.push(character);
                        }
                        _ => return Err(self.error("unknown escape sequence")),
                    }
                }
                _ => output.push(byte as char),
            }
        }
    }

    fn parse_hex4(&mut self) -> Result<u32, String> {
        if self.position + 4 > self.bytes.len() {
            return Err(self.error("truncated unicode escape"));
        }
        let mut value = 0u32;
        for _ in 0..4 {
            let digit = self.bytes[self.position];
            self.position += 1;
            let nibble = match digit {
                b'0'..=b'9' => (digit - b'0') as u32,
                b'a'..=b'f' => (digit - b'a' + 10) as u32,
                b'A'..=b'F' => (digit - b'A' + 10) as u32,
                _ => return Err(self.error("invalid unicode escape")),
            };
            value = (value << 4) | nibble;
        }
        Ok(value)
    }

    fn parse_array(&mut self) -> Result<Json, String> {
        self.position += 1; // '['
        self.depth += 1;
        let mut values = Vec::new();
        loop {
            self.skip_trivia();
            if self.peek() == Some(b']') {
                self.position += 1;
                self.depth -= 1;
                return Ok(Json::Array(values));
            }
            values.push(self.parse_value()?);
            self.skip_trivia();
            match self.peek() {
                Some(b',') => {
                    self.position += 1;
                    self.skip_trivia();
                    if self.peek() == Some(b']') {
                        return Err(self.error("trailing comma in array"));
                    }
                }
                Some(b']') => {
                    self.position += 1;
                    self.depth -= 1;
                    return Ok(Json::Array(values));
                }
                _ => return Err(self.error("expected ',' or ']'")),
            }
        }
    }

    fn parse_object(&mut self) -> Result<Json, String> {
        self.position += 1; // '{'
        self.depth += 1;
        let mut map = BTreeMap::new();
        loop {
            self.skip_trivia();
            if self.peek() == Some(b'}') {
                self.position += 1;
                self.depth -= 1;
                return Ok(Json::Object(map));
            }
            let key = self.parse_string()?;
            self.skip_trivia();
            if self.peek() != Some(b':') {
                return Err(self.error("expected ':'"));
            }
            self.position += 1;
            let value = self.parse_value()?;
            map.insert(key, value);
            self.skip_trivia();
            match self.peek() {
                Some(b',') => {
                    self.position += 1;
                }
                Some(b'}') => {
                    self.position += 1;
                    self.depth -= 1;
                    return Ok(Json::Object(map));
                }
                _ => return Err(self.error("expected ',' or '}'")),
            }
        }
    }
}

pub fn parse(text: &str) -> Result<Json, String> {
    let mut parser = Parser::new(text);
    let value = parser.parse_value()?;
    parser.skip_trivia();
    if parser.position != parser.bytes.len() {
        return Err(parser.error("unexpected trailing data"));
    }
    Ok(value)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_project_style_documents() {
        let document = parse(
            r#"{
                "name": "demo",
                "assets": [{ "path": "models/cube.obj", "type": "mesh" }],
                "count": 3,
                "ratio": 0.5,
                "enabled": true
            }"#,
        )
        .expect("valid json");
        assert_eq!(document.get_str("name"), Some("demo"));
        assert_eq!(document.get_f64("count"), Some(3.0));
        assert_eq!(document.get_bool("enabled"), Some(true));
        let assets = document.get_array("assets").expect("assets array");
        assert_eq!(assets.len(), 1);
        assert_eq!(assets[0].get_str("type"), Some("mesh"));
    }

    #[test]
    fn rejects_invalid_documents() {
        assert!(parse("{").is_err());
        assert!(parse("[1, 2,]").is_err());
        assert!(parse("{\"a\": 1} extra").is_err());
    }

    #[test]
    fn supports_comments_and_escapes() {
        let document = parse("{\n// a comment\n\"text\": \"caf\\u00e9\"\n}").expect("valid");
        assert_eq!(document.get_str("text"), Some("café"));
    }
}
