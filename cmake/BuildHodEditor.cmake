# Shared by both game variants. Serialize access to npm's install tree.
file(MAKE_DIRECTORY "${EDITOR_DIR}/src-tauri/target")
file(LOCK "${EDITOR_DIR}/src-tauri/target/openhw-build.lock" TIMEOUT 600)
find_program(NPM_EXECUTABLE npm.cmd REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)
file(SHA256 "${EDITOR_DIR}/package-lock.json" dependency_hash)
set(stamp "${EDITOR_DIR}/node_modules/.openhw-lock-hash")
if(EXISTS "${stamp}")
    file(READ "${stamp}" installed_hash)
endif()
if(NOT installed_hash STREQUAL dependency_hash)
    execute_process(COMMAND "${NPM_EXECUTABLE}" ci --no-audit --no-fund --ignore-scripts
        WORKING_DIRECTORY "${EDITOR_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    file(WRITE "${stamp}" "${dependency_hash}")
endif()
execute_process(COMMAND "${NPM_EXECUTABLE}" run build
    WORKING_DIRECTORY "${EDITOR_DIR}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CARGO_EXECUTABLE}" build --locked --release 
    --manifest-path src-tauri/Cargo.toml --lib --target-dir src-tauri/target
    WORKING_DIRECTORY "${EDITOR_DIR}" COMMAND_ERROR_IS_FATAL ANY)
get_filename_component(output_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
file(COPY_FILE "${EDITOR_DIR}/src-tauri/target/release/hodeditorjs_lib.dll" "${OUTPUT_FILE}")
