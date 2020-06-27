const { readdirSync, unlinkSync } = require("fs");
const { join } = require("path");
const { exec } = require("child_process");

const SHADER_DIRECTORY = join(__dirname, "assets", "shaders");

// Clear all SPIR-V shaders.
readdirSync(SHADER_DIRECTORY)
    .filter(shader_directory_file => shader_directory_file.endsWith(".spv"))
    .map(spirv_file => join(SHADER_DIRECTORY, spirv_file))
    .forEach(spirv_file_path => unlinkSync(spirv_file_path));

// Compile new SPIR-V shaders.
readdirSync(SHADER_DIRECTORY)
    .map((shader) => ({ shader: join(SHADER_DIRECTORY, shader), output: join(SHADER_DIRECTORY, `${shader}.spv`) }))
    .forEach((command_data) => {
        const GLSLC = "C:\\Users\\joela\\dev\\libs\\VulkanSDK\\1.1.130.0\\Bin32\\glslc.exe";
        const COMMAND = `"${GLSLC}" "${command_data.shader}" -o "${command_data.output}"`;
        exec(COMMAND, (error, stdout, stderr) => {
            console.log(COMMAND);
            if(error) {
                console.error(`\x1b[31m${error.message}\x1b[0m`);
            }
        });
    });
