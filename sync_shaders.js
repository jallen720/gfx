const { readdirSync, unlinkSync } = require("fs");
const { join } = require("path");
const { exec } = require("child_process");
const { homedir } = require("os");

const SHADER_DIRECTORY = join(__dirname, "assets", "shaders");
const GLSLC = join(homedir(), "dev", "libs", "VulkanSDK", "1.1.130.0", "Bin32", "glslc.exe");

function get_dir_ents(dir) {
    let dir_ents = readdirSync(dir, { withFileTypes: true });
    return {
        files: dir_ents.filter(dir_ent => dir_ent.isFile()).map(file => file.name),
        subdirs: dir_ents.filter(dir_ent => dir_ent.isDirectory()).map(Subdirectory => join(dir, Subdirectory.name)),
    };
}

// Clear all SPIR-V shaders.
function clear_spirv_files(dir) {
    let dir_ents = get_dir_ents(dir);
    dir_ents.files
        .filter(file => file.endsWith(".spv"))
        .map(spirv_file => join(dir, spirv_file))
        .forEach(spirv_file_path => unlinkSync(spirv_file_path));
    dir_ents.subdirs.map(clear_spirv_files);
}

// Compile new SPIR-V shaders.
function process_directory(dir) {
    let dir_ents = get_dir_ents(dir);
    dir_ents.files
        .map(shader_src => ({ shader_src: join(dir, shader_src), output: join(dir, `${shader_src}.spv`) }))
        .forEach(cmd_data => {
            let cmd = `"${GLSLC}" "${cmd_data.shader_src}" -o "${cmd_data.output}"`;
            exec(cmd, (err, stdout, stderr) => {
                console.log(cmd);
                if (err)
                    console.error(`\x1b[31m${err.message}\x1b[0m`);
            });
        });
    dir_ents.subdirs.map(process_directory);
}

clear_spirv_files(SHADER_DIRECTORY);
process_directory(SHADER_DIRECTORY);
