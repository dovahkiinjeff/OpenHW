use super::*;
use hwr_hod_parser::hod::HODModel;
use tempfile::tempdir;

#[test]
fn imported_derelict_can_be_created_edited_and_reopened() {
    let dir = tempdir().unwrap();
    let geometry = r##"<COLLADA><library_geometries><geometry id="hull"><mesh>
      <source id="p"><float_array>0 0 0 1 0 0 0 1 0</float_array></source>
      <vertices id="v"><input semantic="POSITION" source="#p"/></vertices>
      <triangles count="1"><input semantic="VERTEX" source="#v" offset="0"/><p>0 1 2</p></triangles>
      </mesh></geometry></library_geometries>
      <library_visual_scenes><visual_scene><node name="Root" id="Root"><instance_geometry url="#hull"/></node></visual_scene></library_visual_scenes>
      </COLLADA>"##;
    let dae = dir.path().join("wreck.dae");
    fs::write(&dae, geometry).unwrap();
    let model = import_dae_file(dae.to_string_lossy().into_owned()).unwrap();
    assert_eq!(model.meshes[0].parts[0].indices.len(), 3);
    let original = dir.path().join("wreck.hod");
    document::save(&original, "", model).unwrap();
    let before = fs::read(&original).unwrap();
    let path = original.to_string_lossy().into_owned();
    let loaded = load_hod(path.clone(), None).unwrap();
    let mut edited: HODModel = serde_json::from_value(serde_json::to_value(loaded).unwrap()).unwrap();
    edited.meshes[0].name = "wreck_salvage".into();
    let copy = dir.path().join("edited.hod");
    document::save(&copy, &path, edited).unwrap();
    let reopened = load_hod(copy.to_string_lossy().into_owned(), None).unwrap();
    assert!(reopened.meshes.iter().any(|m| m.name.contains("wreck_salvage")), "{:?}", reopened.meshes.iter().map(|m| &m.name).collect::<Vec<_>>());
    assert_eq!(reopened.meshes[0].parts[0].indices.len(), 3);
    assert_eq!(before, fs::read(original).unwrap());
}

#[test]
#[ignore = "requires installed models and the game compiler adapter"]
fn mod_workspace_end_to_end(){
    use windows_sys::Win32::System::LibraryLoader::*;
    use hwr_hod_parser::hod::HODJoint;
    let installation=PathBuf::from(std::env::var_os("OPENHW_EDITOR_TEST_ROOT").unwrap());
    let temp=tempfile::tempdir_in(installation.join("Development/Temp")).unwrap();
    fs::hard_link(installation.join("OpenHW-HW1.obig"),temp.path().join("OpenHW-HW1.obig")).unwrap();
    std::env::set_var("OPENHW_EDITOR_TEST_ROOT",temp.path());
    let module=unsafe{LoadLibraryW(std::env::var("OPENHW_EDITOR_TEST_COMPILER").unwrap().encode_utf16().chain(Some(0)).collect::<Vec<_>>().as_ptr())};
    assert!(!module.is_null());
    let complete=unsafe{std::mem::transmute(GetProcAddress(module,b"complete\0".as_ptr()).unwrap())};
    subsystems::COMPLETE.set(Some(complete));
    mod_workspace::create("interceptor_test".into()).unwrap();
    let key="r1/lightinterceptor/rmnative/hull.hod";
    let source=format!("game://{key}");
    let mut model=load_hod(source.clone(),None).unwrap();
    let asset=subsystems::catalog().unwrap().into_iter().find(|s|s.starts_with("kus_")&&s.contains("hvcorvette")).unwrap();
    let rig=subsystems::component(asset.clone()).unwrap();
    let root_name=model.joints.iter().find(|j|!model.joints.iter().any(|p|Some(&p.name)==j.parent_name.as_ref())).unwrap().name.clone();
    let mut root:HODJoint=model.joints[0].clone();root.name="OWMount_1".into();root.parent_name=Some(root_name);
    root.local_transform.m=[[1.,0.,0.,0.],[0.,1.,0.,0.],[0.,0.,1.,0.],[0.,0.,0.,1.]];
    root.position=None;root.rotation=None;
    model.joints.push(root);
    for (i,joint) in rig.joints.iter().enumerate(){
        let mut joint=joint.clone();
        let parent=rig.joints.iter().position(|j|Some(&j.name)==joint.parent_name.as_ref());
        joint.name=format!("OWMount_1_J{i}");
        joint.parent_name=Some(parent.map(|i|format!("OWMount_1_J{i}")).unwrap_or("OWMount_1".into()));
        model.joints.push(joint);
    }
    let mounts=vec![subsystems::Mount{slot:"OWMount_1".into(),asset,anchor:"OWMount_1".into(),authored:true}];
    let path=mod_workspace::save("interceptor_test".into(),source,model,mounts,key.into()).unwrap();
    assert!(path.replace('\\',"/").ends_with("mods/interceptor_test/r1/lightinterceptor/rmnative/hull.hod"),"{path}");
    let related=mod_workspace::files(path.clone()).unwrap();
    assert!(related.iter().any(|f|f.kind=="events"));
    assert!(related.iter().any(|f|f.key.ends_with(".wf")));
    let turret_events=related.iter().find(|f|f.kind=="events" && f.key.starts_with("subsystems/")).expect("turret events");
    let document:serde_json::Value=serde_json::from_str(&mod_workspace::read_text(path.clone(),turret_events.key.clone()).unwrap()).unwrap();
    assert!(!document["timelines"].as_array().unwrap().is_empty());
    assert!(document["events"].as_array().unwrap().iter().any(|e|e["fx"].as_str().is_some_and(|s|!s.is_empty())));
    for file in related {
        let text=mod_workspace::read_text(path.clone(),file.key.clone()).unwrap();
        mod_workspace::write_text(path.clone(),file.key.clone(),text.clone()).unwrap();
        assert_eq!(mod_workspace::read_text(path.clone(),file.key).unwrap(),text);
    }
    let definition=fs::read_to_string(Path::new(&path).with_file_name("ship.owshipdef")).unwrap();
    assert!(definition.contains("weaponSlot OWMount_1 0 "));
    assert!(definition.contains("surface 0 0 ow_m0"));
    assert!(Path::new(&path).with_file_name("ow_m0_material.dds").exists());
    assert!(definition.contains("hardpoint HP_OWMount_1 OWMount_1"));
    let validate:unsafe extern "C" fn(*const u16,*const u8,usize,*mut i8,usize)->i32=unsafe{std::mem::transmute(GetProcAddress(module,b"validate_saved\0".as_ptr()).unwrap())};
    let directory=Path::new(&path).parent().unwrap().to_string_lossy().encode_utf16().chain(Some(0)).collect::<Vec<_>>();
    let mut error=[0i8;512];
    assert_eq!(unsafe{validate(directory.as_ptr(),definition.as_ptr(),definition.len(),error.as_mut_ptr(),error.len())},1,
        "Native game rejected mod ship: {}",unsafe{std::ffi::CStr::from_ptr(error.as_ptr())}.to_string_lossy());
    assert!(fs::read_to_string(temp.path().join("mods/load-order.txt")).unwrap().contains("interceptor_test"));
    assert!(mod_workspace::create("../escape".into()).is_err());
    std::env::set_var("OPENHW_EDITOR_TEST_ROOT",installation);
}
