//! The game owns the window, message pump, and this child WebView's lifetime.
use super::*;
use raw_window_handle::*;
use serde_json::{json, Value};
#[cfg(test)]
use std::cell::Cell;
use std::{borrow::Cow, cell::RefCell, collections::VecDeque, num::NonZeroIsize, rc::Rc};
use windows_sys::Win32::UI::WindowsAndMessaging::GetClientRect;
use wry::{
    dpi::{PhysicalPosition, PhysicalSize},
    http::Response,
    Rect, WebView, WebViewBuilder,
};
include!(concat!(env!("OUT_DIR"), "/assets.rs"));

#[derive(Clone, Copy)]
pub struct GameWindow(NonZeroIsize);
impl HasWindowHandle for GameWindow {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        Ok(unsafe { WindowHandle::borrow_raw(Win32WindowHandle::new(self.0).into()) })
    }
}
impl HasDisplayHandle for GameWindow {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        Ok(DisplayHandle::windows())
    }
}
pub fn root() -> Result<PathBuf, String> {
    #[cfg(test)]
    if let Some(path) = std::env::var_os("OPENHW_EDITOR_TEST_ROOT") {
        return Ok(path.into());
    }
    std::env::current_exe()
        .map_err(|e| e.to_string())?
        .parent()
        .map(Path::to_owned)
        .ok_or("Missing game folder".into())
}
#[derive(Deserialize)]
struct Request {
    id: u64,
    command: String,
    args: Value,
}
struct Editor {
    view: WebView,
    parent: GameWindow,
    size: (i32, i32),
    queue: Rc<RefCell<VecDeque<Request>>>,
}
thread_local! { static EDITOR: RefCell<Option<Editor>> = const {RefCell::new(None)}; }
fn dimensions(parent: GameWindow) -> (i32, i32) {
    let mut r = Default::default();
    unsafe { GetClientRect(parent.0.get() as _, &mut r) };
    (r.right.max(1), r.bottom.max(1))
}
fn bounds(size: (i32, i32)) -> Rect {
    Rect {
        position: PhysicalPosition::new(0, 0).into(),
        size: PhysicalSize::new(size.0 as u32, size.1 as u32).into(),
    }
}
fn open(parent: GameWindow) -> Result<Editor, String> {
    let queue = Rc::new(RefCell::new(VecDeque::new()));
    let incoming = Rc::clone(&queue);
    let size = dimensions(parent);
    let profile =
        PathBuf::from(std::env::var_os("LOCALAPPDATA").ok_or("No local application folder")?)
            .join("OpenHW/HodWorkshop");
    let mut context = wry::WebContext::new(Some(profile));
    let view=WebViewBuilder::new_with_web_context(&mut context)
        .with_custom_protocol("workshop".into(), |_, req| {
            let path=req.uri().path().trim_start_matches('/');
            let key=if path.is_empty() {"index.html"} else {path};
            match asset(key) {
                Some((mime,data))=>Response::builder().header("Content-Type",mime)
                    .header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self' data: blob:; object-src 'none'; frame-src 'none'; base-uri 'none'")
                    .body(Cow::Borrowed(data)).unwrap(),
                None=>Response::builder().status(404).body(Cow::Borrowed(&[][..])).unwrap()
            }
        })
        .with_navigation_handler(|url| url == "http://workshop.localhost/" || url == "workshop://localhost/")
        .with_new_window_req_handler(|_,_| wry::NewWindowResponse::Deny)
        .with_ipc_handler(move |msg| {
            if let Ok(request)=serde_json::from_str(msg.body()) {incoming.borrow_mut().push_back(request);}
        }).with_bounds(bounds(size)).with_url("workshop://localhost/")
        .build_as_child(&parent).map_err(|e|e.to_string())?;
    view.focus().map_err(|e| e.to_string())?;
    Ok(Editor {
        view,
        parent,
        size,
        queue,
    })
}
fn dispatch(window: GameWindow, cmd: &str, mut args: Value) -> Result<Value, String> {
    macro_rules! arg {
        ($key:literal) => {
            serde_json::from_value(args.get_mut($key).map(Value::take).unwrap_or(Value::Null))
                .map_err(|e| format!("{}: {}", $key, e))?
        };
    }
    Ok(match cmd {
        #[cfg(test)]
        "qa_error" => {
            QA_ERROR.set(Some(arg!("text")));
            Value::Null
        }
        "log_event" => {
            log_event(arg!("level"), arg!("message"));
            Value::Null
        }
        "related_files" => json!(mod_workspace::files(arg!("path"))?),
        "read_related_file" => json!(mod_workspace::read_text(arg!("path"),arg!("key"))?),
        "write_related_file" => {mod_workspace::write_text(arg!("path"),arg!("key"),arg!("text"))?;Value::Null},
        "list_mods" => json!(mod_workspace::list()?),
        "create_mod" => json!(mod_workspace::create(arg!("name"))?),
        "save_to_mod" => json!(mod_workspace::save(arg!("name"),arg!("sourcePath"),arg!("model"),arg!("mounts"),arg!("template"))?),
        "list_game_models" => json!(list_game_models()?),
        "load_hod" => json!(load_hod(arg!("filePath"), arg!("keeperPath"))?),
        "save_hod" => {
            subsystems::save(
                arg!("targetPath"),
                arg!("sourcePath"),
                arg!("model"),
                arg!("mounts"),
                arg!("template"),
            )?;
            Value::Null
        }
        "list_subsystems" => json!(subsystems::catalog()?),
        "subsystem_model" => json!(subsystems::component(arg!("name"))?),
        "ship_mounts" => json!(subsystems::read_mounts(arg!("source"))?),
        "select_hod_file" => json!(select_hod_file(window)?),
        "select_dae_file" => json!(select_dae_file(window)?),
        "select_shader_directory" => json!(select_shader_directory(window)?),
        "select_save_hod_file" => json!(select_save_hod_file(window, arg!("defaultName"))?),
        "load_shader_config" => json!(load_shader_config()?),
        "save_shader_config" => json!(save_shader_config(arg!("config"))?),
        "get_dynamic_shader_slots" => json!(get_dynamic_shader_slots(arg!("keeperPaths"))?),
        "get_shader_pipelines" => json!(get_shader_pipelines(arg!("keeperPaths"))?),
        "export_textures_tga" => json!(export_textures_tga(arg!("folderPath"), arg!("textures"))?),
        "import_tga_textures" => json!(import_tga_textures(window)?),
        "load_text_file" => json!(load_text_file(window, arg!("filters"))?),
        "save_text_file" => json!(save_text_file(
            window,
            arg!("defaultName"),
            arg!("filters"),
            arg!("contents")
        )?),
        "import_dae_file" => json!(import_dae_file(arg!("path"))?),
        "convert_weapon_to_turret" => {
            json!(convert_weapon_to_turret(arg!("model"), arg!("baseName"))?)
        }
        "auto_generate_collision_from_mesh" => json!(auto_generate_collision_from_mesh(
            arg!("model"),
            arg!("collisionMeshName"),
            arg!("sourceMeshName"),
            arg!("sourceMeshNames")
        )?),
        _ => return Err(format!("Unknown editor action {cmd}")),
    })
}
#[no_mangle]
pub extern "C" fn hod_open(hwnd: isize, complete: subsystems::Complete) -> i32 {
    let Some(raw) = NonZeroIsize::new(hwnd) else {
        return 0;
    };
    subsystems::COMPLETE.set(Some(complete));
    match open(GameWindow(raw)) {
        Ok(view) => {
            EDITOR.with(|s| *s.borrow_mut() = Some(view));
            1
        }
        Err(e) => {
            write_log("ERROR", &e);
            0
        }
    }
}
#[no_mangle]
pub extern "C" fn hod_close() {
    EDITOR.with(|s| {
        if let Ok(mut state) = s.try_borrow_mut() {
            state.take();
        }
    });
    if let Ok(mut source) = document::SOURCE.lock() {
        *source = None;
    }
}
#[no_mangle]
pub extern "C" fn hod_tick() -> i32 {
    EDITOR.with(|slot| {
        let Ok(mut state) = slot.try_borrow_mut() else {
            return 1;
        };
        let Some(editor) = state.as_mut() else {
            return 0;
        };
        let size = dimensions(editor.parent);
        if size != editor.size {
            let _ = editor.view.set_bounds(bounds(size));
            editor.size = size;
        }
        let requests = std::mem::take(&mut *editor.queue.borrow_mut());
        for r in requests {
            if r.command == "close_workshop" {
                *state = None;
                if let Ok(mut source) = document::SOURCE.lock() {
                    *source = None;
                }
                return 0;
            }
            let result = dispatch(editor.parent, &r.command, r.args);
            let (ok, value) = match result {
                Ok(v) => (true, v),
                Err(e) => (false, json!(e)),
            };
            let _ = editor
                .view
                .evaluate_script(&format!("window.openhwReply({},{},{})", r.id, ok, value));
        }
        1
    })
}

#[cfg(test)]
thread_local! {static QA_ERROR:Cell<Option<String>>=const{Cell::new(None)};}
#[cfg(test)]
#[path = "embedded_tests.rs"]
mod embedded_tests;
