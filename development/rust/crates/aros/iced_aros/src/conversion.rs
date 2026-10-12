//! Window events (`ffi::PcEvent`) as iced events.

use crate::core::keyboard::{self, key, Key, Modifiers};
use crate::core::mouse;
use crate::core::window;
use crate::core::{Event, Point, Size, SmolStr};
use crate::ffi::{self, PcEvent};

/// What the window's events amount to between two passes.
#[derive(Default)]
pub struct Input {
    pub events: Vec<Event>,
    pub modifiers: Modifiers,
    pub cursor: Option<Point>,
    pub resized: Option<(i32, i32)>,
    pub close_requested: bool,
    pub focus: Option<bool>,
}

impl Input {
    pub fn modifiers_from(qualifier: u32) -> Modifiers {
        let mut m = Modifiers::empty();
        if qualifier & (ffi::Q_LSHIFT | ffi::Q_RSHIFT) != 0 {
            m |= Modifiers::SHIFT;
        }
        if qualifier & ffi::Q_CONTROL != 0 {
            m |= Modifiers::CTRL;
        }
        if qualifier & (ffi::Q_LALT | ffi::Q_RALT) != 0 {
            m |= Modifiers::ALT;
        }
        if qualifier & (ffi::Q_LCOMMAND | ffi::Q_RCOMMAND) != 0 {
            m |= Modifiers::LOGO;
        }
        m
    }

    /// Folds one window event in; positions are in logical points (`scale` pixels per point).
    pub fn push(&mut self, ev: &PcEvent, scale: f32) {
        let modifiers = Self::modifiers_from(ev.qualifier);
        self.modifiers = modifiers;
        let position = Point::new(ev.x as f32 / scale, ev.y as f32 / scale);
        match ev.kind {
            ffi::EV_CLOSE => self.close_requested = true,
            ffi::EV_RESIZE => self.resized = Some((ev.width, ev.height)),
            ffi::EV_FOCUS => {
                let focused = ev.focus != 0;
                self.focus = Some(focused);
                self.events.push(Event::Window(if focused { window::Event::Focused } else { window::Event::Unfocused }));
                if !focused {
                    self.events.push(Event::Mouse(mouse::Event::CursorLeft));
                    self.cursor = None;
                }
            }
            ffi::EV_MOUSE_MOVE => {
                if self.cursor.is_none() {
                    self.events.push(Event::Mouse(mouse::Event::CursorEntered));
                }
                self.cursor = Some(position);
                self.events.push(Event::Mouse(mouse::Event::CursorMoved { position }));
            }
            ffi::EV_BUTTON => {
                let button = match ev.button {
                    0 => mouse::Button::Left,
                    1 => mouse::Button::Right,
                    _ => mouse::Button::Middle,
                };
                if self.cursor.is_none() {
                    self.events.push(Event::Mouse(mouse::Event::CursorEntered));
                    self.events.push(Event::Mouse(mouse::Event::CursorMoved { position }));
                }
                self.cursor = Some(position);
                self.events.push(Event::Mouse(if ev.down != 0 {
                    mouse::Event::ButtonPressed(button)
                } else {
                    mouse::Event::ButtonReleased(button)
                }));
            }
            ffi::EV_WHEEL => {
                self.events.push(Event::Mouse(mouse::Event::WheelScrolled {
                    delta: mouse::ScrollDelta::Lines { x: ev.wheel_x as f32, y: ev.wheel_y as f32 },
                }));
            }
            ffi::EV_KEY => self.key(ev, modifiers),
            _ => {}
        }
    }

    fn key(&mut self, ev: &PcEvent, modifiers: Modifiers) {
        let pressed = ev.down != 0;
        let repeat = ev.qualifier & ffi::Q_REPEAT != 0;
        let plain = ev.plain();
        let typed = ev.text();
        let (key, location) = key_for(ev.rawkey, &plain);
        let Some(key) = key else { return };
        // The key as typed: the character with Shift and the dead keys applied, when the keymap
        // gives one; Control and Amiga combinations keep the plain key.
        let modified_key = match (&key, typed.chars().next()) {
            (Key::Character(_), Some(c)) if !c.is_control() && !modifiers.control() && !modifiers.logo() => Key::Character(SmolStr::new(typed.as_str())),
            _ => key.clone(),
        };
        let text = (pressed && !modifiers.control() && !modifiers.logo() && !modifiers.alt())
            .then(|| {
                let t: String = typed.chars().filter(|c| !c.is_control() || *c == '\r' || *c == '\n' || *c == '\t').collect();
                (!t.is_empty()).then(|| SmolStr::new(t))
            })
            .flatten();
        let physical_key = key::Physical::Unidentified(key::NativeCode::Unidentified);
        self.events.push(Event::Keyboard(if pressed {
            keyboard::Event::KeyPressed { key, modified_key, physical_key, location, modifiers, text, repeat }
        } else {
            keyboard::Event::KeyReleased { key, modified_key, physical_key, location, modifiers }
        }));
    }

    pub fn take_events(&mut self) -> Vec<Event> {
        std::mem::take(&mut self.events)
    }
}

/// Logical size of the window's inner area in points.
pub fn logical_size(width: i32, height: i32, scale: f32) -> Size {
    Size::new(width as f32 / scale, height as f32 / scale)
}

/// The logical key for a raw key code: the special keys by code (the same on every keymap),
/// the rest from the character the keymap gives the plain key.
fn key_for(rawkey: i32, plain: &str) -> (Option<Key>, keyboard::Location) {
    use key::Named;
    let named = |n: Named| (Some(Key::Named(n)), keyboard::Location::Standard);
    match rawkey {
        0x41 => return named(Named::Backspace),
        0x42 => return named(Named::Tab),
        0x43 => return (Some(Key::Named(Named::Enter)), keyboard::Location::Numpad),
        0x44 => return named(Named::Enter),
        0x45 => return named(Named::Escape),
        0x46 => return named(Named::Delete),
        0x47 => return named(Named::Insert),
        0x48 => return named(Named::PageUp),
        0x49 => return named(Named::PageDown),
        0x4c => return named(Named::ArrowUp),
        0x4d => return named(Named::ArrowDown),
        0x4e => return named(Named::ArrowRight),
        0x4f => return named(Named::ArrowLeft),
        0x50 => return named(Named::F1),
        0x51 => return named(Named::F2),
        0x52 => return named(Named::F3),
        0x53 => return named(Named::F4),
        0x54 => return named(Named::F5),
        0x55 => return named(Named::F6),
        0x56 => return named(Named::F7),
        0x57 => return named(Named::F8),
        0x58 => return named(Named::F9),
        0x59 => return named(Named::F10),
        0x4b => return named(Named::F11),
        0x6f => return named(Named::F12),
        0x5f => return named(Named::Help),
        0x70 => return named(Named::Home),
        0x71 => return named(Named::End),
        0x40 => return named(Named::Space),
        0x60 => return (Some(Key::Named(Named::Shift)), keyboard::Location::Left),
        0x61 => return (Some(Key::Named(Named::Shift)), keyboard::Location::Right),
        0x62 => return named(Named::CapsLock),
        0x63 => return (Some(Key::Named(Named::Control)), keyboard::Location::Left),
        0x64 => return (Some(Key::Named(Named::Alt)), keyboard::Location::Left),
        0x65 => return (Some(Key::Named(Named::Alt)), keyboard::Location::Right),
        0x66 => return (Some(Key::Named(Named::Super)), keyboard::Location::Left),
        0x67 => return (Some(Key::Named(Named::Super)), keyboard::Location::Right),
        _ => {}
    }
    let c = plain.chars().next();
    match c {
        Some(c) if !c.is_control() => (Some(Key::Character(SmolStr::new(c.to_string()))), keyboard::Location::Standard),
        _ => (None, keyboard::Location::Standard),
    }
}
