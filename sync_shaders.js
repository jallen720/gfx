const { readdirSync, unlinkSync } = require("fs");
const { join } = require("path");
const { exec } = require("child_process");
const { homedir } = require("os");

const SHADER_DIRECTORY = join(__dirname, "assets", "shaders");
const GLSLC = join(homedir(), "dev", "libs", "VulkanSDK", "1.1.130.0", "Bin32", "glslc.exe");

function get_directory_entities(Directory) {
    const DirectoryEntities = readdirSync(Directory, { withFileTypes: true });
    return {
        Files: DirectoryEntities.filter(DirectoryEntity => DirectoryEntity.isFile()).map(File => File.name),
        Subdirectories:
            DirectoryEntities
                .filter(DirectoryEntity => DirectoryEntity.isDirectory())
                .map(Subdirectory => join(Directory, Subdirectory.name)),
    };
}

// Clear all SPIR-V shaders.
function clear_spirv_files(Directory) {
    const DirectoryEntities = get_directory_entities(Directory);
    DirectoryEntities.Files
        .filter(File => File.endsWith(".spv"))
        .map(SPIRVFile => join(Directory, SPIRVFile))
        .forEach(SPIRVFilePath => unlinkSync(SPIRVFilePath));
    DirectoryEntities.Subdirectories.map(clear_spirv_files);
}

// Compile new SPIR-V shaders.
function process_directory(Directory) {
    const DirectoryEntities = get_directory_entities(Directory);
    DirectoryEntities.Files
        .map(ShaderSource => ({ ShaderSource: join(Directory, ShaderSource), Output: join(Directory, `${ShaderSource}.spv`) }))
        .forEach(CommandData => {
            const COMMAND = `"${GLSLC}" "${CommandData.ShaderSource}" -o "${CommandData.Output}"`;
            exec(COMMAND, (Error, Stdout, Stderr) => {
                console.log(COMMAND);
                if(Error) {
                    console.error(`\x1b[31m${Error.message}\x1b[0m`);
                }
            });
        });
    DirectoryEntities.Subdirectories.map(process_directory);
}

clear_spirv_files(SHADER_DIRECTORY);
process_directory(SHADER_DIRECTORY);
