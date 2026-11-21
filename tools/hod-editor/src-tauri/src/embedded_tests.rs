use super::*;
use std::time::{Duration, Instant};
use windows_sys::Win32::{System::LibraryLoader::*, UI::WindowsAndMessaging::*};

// Uses the installed game's real HOD archive and a test adapter for its native
// ship-definition compiler. The visible desktop is never used by this test.
#[test]
#[ignore = "requires an OpenHW installation and its gameplay compiler test adapter"]
fn embedded_quick_add_copy_save_and_reopen() {
    let temp = tempfile::tempdir().unwrap();
    let path = temp.path().join("interceptor.hod");
    std::env::set_var("OPENHW_EDITOR_TEST_SAVE", &path);
    let dll = std::env::var("OPENHW_EDITOR_TEST_COMPILER").unwrap();
    let wide = |s: &str| s.encode_utf16().chain(Some(0)).collect::<Vec<_>>();
    let name = wide("STATIC");
    let validate: unsafe extern "C" fn(*const u16, *const u8, usize, *mut i8, usize) -> i32;
    unsafe {
        let window = CreateWindowExW(
            0,
            name.as_ptr(),
            name.as_ptr(),
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            1300,
            800,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::null(),
        );
        assert!(!window.is_null());
        let module = LoadLibraryW(wide(&dll).as_ptr());
        validate =
            std::mem::transmute(GetProcAddress(module, b"validate_saved\0".as_ptr()).unwrap());
        let complete = std::mem::transmute(GetProcAddress(module, b"complete\0".as_ptr()).unwrap());
        assert_eq!(hod_open(window as isize, complete), 1);
        assert!(!GetWindow(window, GW_CHILD).is_null());
        let start = Instant::now();
        loop {
            let mut msg = std::mem::zeroed();
            while PeekMessageW(&mut msg, std::ptr::null_mut(), 0, 0, PM_REMOVE) != 0 {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            EDITOR.with(|s| {
                s.borrow()
                    .as_ref()
                    .unwrap()
                    .view
                    .evaluate_script(include_str!("embedded_test.js"))
                    .unwrap()
            });
            if hod_tick() == 0 {
                break;
            }
            if let Some(err) = QA_ERROR.take() {
                panic!("{err}");
            }
            assert!(
                start.elapsed() < Duration::from_secs(75),
                "Timed out waiting for the embedded editor"
            );
            std::thread::sleep(Duration::from_millis(25));
        }
        assert!(GetWindow(window, GW_CHILD).is_null());
        assert_eq!(hod_open(window as isize, complete), 1);
        EDITOR.with(|s| {
            s.borrow_mut().take();
        });
        DestroyWindow(window);
    }
    let definition = fs::read_to_string(path.with_extension("owshipdef")).unwrap();
    let mut error = [0i8; 512];
    assert_eq!(
        unsafe {
            validate(
                wide(temp.path().to_str().unwrap()).as_ptr(),
                definition.as_ptr(),
                definition.len(),
                error.as_mut_ptr(),
                error.len(),
            )
        },
        1,
        "Game renderer rejected saved ship: {}",
        unsafe { std::ffi::CStr::from_ptr(error.as_ptr()) }.to_string_lossy()
    );
    for slot in ["OWMount_1", "OWMount_2", "OWMount_3"] {
        assert!(definition.contains(&format!("hardpoint HP_{slot} {slot}")));
        assert!(definition.contains(&format!("weaponSlot {slot} 0 ")));
        assert!(definition.contains(&format!("joint {slot} ")));
    }
    let ship = load_hod(path.to_string_lossy().into_owned(), None).unwrap();
    assert!(ship.joints.iter().any(|j| j.name == "OWMount_2_J0"));
    let bindings = subsystems::read_mounts(path.to_string_lossy().into_owned()).unwrap();
    assert_eq!(bindings["mounts"].as_array().unwrap().len(), 3);
}
