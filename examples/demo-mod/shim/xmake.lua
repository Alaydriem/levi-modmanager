add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina 26.51.6", {configs = {target_type = "client"}})
add_requires("levibuildscript 0.6.2")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("alaydriem-demo")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker", {modName = "alaydriem-demo", modVersion = "0.1.0"})
    add_packages("levilamina")
    add_syslinks("d3d11", "d3d12", "dxgi", "d3dcompiler", "user32", "kernel32", "ntdll", "userenv", "ws2_32", "dbghelp", "bcrypt", "advapi32", "ole32", "shell32")
    add_defines("NOMINMAX", "UNICODE", "_AMD64_")
    set_exceptions("none")
    add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_cxflags(
        "/EHs",
        "-Wno-microsoft-cast",
        "-Wno-invalid-offsetof",
        "-Wno-c++2b-extensions",
        "-Wno-microsoft-include",
        "-Wno-overloaded-virtual",
        "-Wno-ignored-qualifiers",
        "-Wno-missing-field-initializers",
        "-Wno-potentially-evaluated-expression",
        "-Wno-pragma-system-header-outside-header",
        {tools = {"clang_cl"}}
    )
    set_toolchains("clang-cl")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h", "include/demo_ffi.h", "../../../include/alaydriem_mod_manager.h")
    add_files("src/**.cpp")
    add_includedirs("src", "include", "../../../include")
    add_defines("LL_PLAT_C")
    add_linkdirs("$(projectdir)/../../../target/release")
    add_links("demo_ffi")
    -- the DLL embeds Nordwand Mono, so the packaged mod ships the font licence beside it
    after_build(function (target)
        local outputdir = path.join(os.projectdir(), "bin", "alaydriem-demo")
        os.mkdir(outputdir)
        os.cp(path.join(os.projectdir(), "..", "..", "..", "..", "levi-crates", "crates", "levi-ui", "assets", "fonts", "OFL.txt"), path.join(outputdir, "OFL.txt"))
    end)
