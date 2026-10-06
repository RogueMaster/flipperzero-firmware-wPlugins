//! Windows tray icon (tray-icon + tao). No balloon/toast notifications are ever shown.

use crate::app;
use crate::config::{config_dir, log_path, TransportMode};
use crate::icon::{self, IconState};
use crate::shared::Shared;
use std::process::Command;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tao::event::{Event, StartCause};
use tao::event_loop::{ControlFlow, EventLoopBuilder};
use tray_icon::menu::{CheckMenuItem, Menu, MenuEvent, MenuItem, PredefinedMenuItem, Submenu};
use tray_icon::{Icon, TrayIcon, TrayIconBuilder};

enum UserEvent {
    Menu(MenuEvent),
}

const ICON_SIZE: u32 = 32;
/// Windows truncates tray tooltips at 127 UTF-16 units.
const TOOLTIP_MAX: usize = 120;

fn make_icon(state: IconState) -> Option<Icon> {
    Icon::from_rgba(icon::render(ICON_SIZE, state), ICON_SIZE, ICON_SIZE).ok()
}

fn tooltip_text(shared: &Shared) -> String {
    let mut t = format!("PC Health Remote\n{}\n{}", shared.status_line(), shared.summary());
    if t.chars().count() > TOOLTIP_MAX {
        t = t.chars().take(TOOLTIP_MAX - 1).collect::<String>() + "\u{2026}";
    }
    t
}

fn state_of(shared: &Shared) -> IconState {
    if shared.pairing() {
        IconState::Pairing
    } else if shared.link().is_some() {
        IconState::Connected
    } else {
        IconState::Waiting
    }
}

fn open_in_explorer(path: &std::path::Path) {
    let mut cmd = Command::new("explorer.exe");
    if let Err(e) = crate::winutil::hide_window(&mut cmd).arg(path).spawn() {
        log::warn!("cannot open {}: {e}", path.display());
    }
}

/// Run the tray event loop until "Quit". Never returns (tao exits the process).
pub fn run(shared: Arc<Shared>, on_quit: Box<dyn FnOnce() + Send>) -> ! {
    let event_loop = EventLoopBuilder::<UserEvent>::with_user_event().build();
    let proxy = event_loop.create_proxy();
    MenuEvent::set_event_handler(Some(move |e| {
        let _ = proxy.send_event(UserEvent::Menu(e));
    }));

    // Menu.
    let status_item = MenuItem::with_id("status", shared.status_line(), false, None);
    let pair_item = MenuItem::with_id("pair", "Pair new Flipper\u{2026}", true, None);
    let mode = shared.transport();
    let t_auto = CheckMenuItem::with_id("t_auto", "Auto", true, mode == TransportMode::Auto, None);
    let t_ble = CheckMenuItem::with_id("t_ble", "BLE", true, mode == TransportMode::Ble, None);
    let t_usb = CheckMenuItem::with_id("t_usb", "USB", true, mode == TransportMode::Usb, None);
    let transport_menu = Submenu::new("Transport", true);
    let config_item = MenuItem::with_id("config", "Open config folder", true, None);
    let log_item = MenuItem::with_id("log", "Open log", true, None);
    let quit_item = MenuItem::with_id("quit", "Quit", true, None);

    let menu = Menu::new();
    let build = || -> Result<(), tray_icon::menu::Error> {
        transport_menu.append(&t_auto)?;
        transport_menu.append(&t_ble)?;
        transport_menu.append(&t_usb)?;
        menu.append(&status_item)?;
        menu.append(&PredefinedMenuItem::separator())?;
        menu.append(&pair_item)?;
        menu.append(&transport_menu)?;
        menu.append(&PredefinedMenuItem::separator())?;
        menu.append(&config_item)?;
        menu.append(&log_item)?;
        menu.append(&PredefinedMenuItem::separator())?;
        menu.append(&quit_item)
    };
    if let Err(e) = build() {
        log::error!("cannot build tray menu: {e}");
    }

    let mut tray: Option<TrayIcon> = None;
    let mut shown_state: Option<IconState> = None;
    let mut shown_tooltip = String::new();
    let mut shown_status = String::new();
    let mut on_quit = Some(on_quit);
    let mut last_refresh = Instant::now() - Duration::from_secs(10);

    event_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::WaitUntil(Instant::now() + Duration::from_millis(1000));

        match event {
            Event::NewEvents(StartCause::Init) => {
                let mut b =
                    TrayIconBuilder::new().with_menu(Box::new(menu.clone())).with_tooltip(tooltip_text(&shared));
                if let Some(i) = make_icon(state_of(&shared)) {
                    b = b.with_icon(i);
                }
                match b.build() {
                    Ok(t) => tray = Some(t),
                    Err(e) => log::error!("cannot create tray icon: {e}"),
                }
                shown_state = Some(state_of(&shared));
            }
            Event::UserEvent(UserEvent::Menu(e)) => {
                let set_mode = |m: TransportMode| {
                    app::set_transport(&shared, m);
                    t_auto.set_checked(m == TransportMode::Auto);
                    t_ble.set_checked(m == TransportMode::Ble);
                    t_usb.set_checked(m == TransportMode::Usb);
                };
                if e.id == "pair" {
                    app::pair_in_background(shared.clone());
                } else if e.id == "t_auto" {
                    set_mode(TransportMode::Auto);
                } else if e.id == "t_ble" {
                    set_mode(TransportMode::Ble);
                } else if e.id == "t_usb" {
                    set_mode(TransportMode::Usb);
                } else if e.id == "config" {
                    open_in_explorer(&config_dir());
                } else if e.id == "log" {
                    open_in_explorer(&log_path());
                } else if e.id == "quit" {
                    log::info!("quit requested from tray");
                    shared.stop.store(true, std::sync::atomic::Ordering::Relaxed);
                }
            }
            _ => {}
        }

        if shared.stopping() {
            // Remove the icon first so no ghost is left in the notification area.
            tray = None;
            if let Some(f) = on_quit.take() {
                f();
            }
            *control_flow = ControlFlow::Exit;
            return;
        }

        if last_refresh.elapsed() >= Duration::from_millis(900) {
            last_refresh = Instant::now();
            let status = shared.status_line();
            if status != shown_status {
                status_item.set_text(&status);
                shown_status = status;
            }
            if let Some(t) = tray.as_ref() {
                let tip = tooltip_text(&shared);
                if tip != shown_tooltip {
                    let _ = t.set_tooltip(Some(&tip));
                    shown_tooltip = tip;
                }
                let st = state_of(&shared);
                if shown_state != Some(st) {
                    shown_state = Some(st);
                    if let Some(i) = make_icon(st) {
                        let _ = t.set_icon(Some(i));
                    }
                }
            }
        }
    })
}
