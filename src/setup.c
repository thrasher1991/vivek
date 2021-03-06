/* Copyright (c) 2010-2019 Sander Mertens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "bake.h"

#ifndef _WIN32
#define GLOBAL_PATH "/usr/local/bin"
#else
#define GLOBAL_PATH "C:\\Windows\\system32"
#endif

static
int16_t cmd(
   char *cmd)
{
    int8_t ret;
    int sig;
    if (ut_log_verbosityGet() <= UT_TRACE) {
        sig = ut_proc_cmd(cmd, &ret);
    } else {
#ifdef _WIN32
        char *cmd_redirect = ut_asprintf("%s > nul", cmd);
        ret = system(cmd_redirect);
        sig = 0;
#else
        sig = ut_proc_cmd_stderr_only(cmd, &ret);
#endif
    }
    
    if (sig || ret) {
        ut_catch();
        bake_message(UT_ERROR, "error", "'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

#ifdef _WIN32
int16_t bake_create_script(void)
{
    char *script_path = ut_envparse("$USERPROFILE\\bake\\bake.bat");
    if (!script_path) {
        ut_error("missing $USERPROFILE environment variable");
        goto error;
    }

    char *vc_shell_cmd = ut_get_vc_shell_cmd();

    FILE *f = fopen(script_path, "wb");

    if (!f) {
        ut_error("cannot open '%s': %s", script_path, strerror(errno));
        goto error;
    }

    fprintf(f, "@ECHO OFF\n\n");
    fprintf(f, "where nmake > nul\n");
    fprintf(f, "IF %%ERRORLEVEL%% == 1 (\n");
    fprintf(f, "    %s\n", vc_shell_cmd);
    fprintf(f, ")\n");
    fprintf(f, "IF [%%1] == [upgrade] (\n");
    fprintf(f, "    mkdir %%USERPROFILE%%\\bake\\src\n");
    fprintf(f, "    cd %%USERPROFILE%%\\bake\\src\n\n");
    fprintf(f, "    IF NOT EXIST \"bake\" (\n");
    fprintf(f, "        echo \"Cloning bake repository...\"\n");
    fprintf(f, "        git clone -q \"https://github.com/SanderMertens/bake.git\"\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "    ) else (\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "        echo \"Reset bake repository...\"\n");
    fprintf(f, "        git fetch -q origin\n");
    fprintf(f, "        git reset -q --hard origin/master\n");
    fprintf(f, "        git clean -q -xdf\n");
    fprintf(f, "    )\n");
    fprintf(f, "\n\n");
    fprintf(f, "    cd build-Windows\\\n");
    fprintf(f, "    nmake /f bake.mak\n");
    fprintf(f, "\n\n");
    fprintf(f, "    bake.exe setup --local-setup\n");
    fprintf(f, "\n\n");
    fprintf(f, ") ELSE (\n");
    fprintf(f, "    cmd /c %%USERPROFILE%%\\bake\\"BAKE_EXEC".exe %%*\n");
    fprintf(f, ")\n\n");

    fclose(f);

    free(vc_shell_cmd);

    /* Copy file to global location, may ask for password */
    bake_message(UT_WARNING, "WARNING", 
        "#[normal]copying script to '" GLOBAL_PATH "', #[yellow]requires administrator privileges");

    if (ut_cp(script_path, GLOBAL_PATH)) {
        printf("\n");
        bake_message(UT_WARNING, "",
            "Failed to instal bake script to %s. Setup will continue, but you will\n"
            "need to manually add %%USERPROFILE%%\\bake to the %%PATH%% environment variable.\n"
            "Alternatively, you can retry the setup with administrator privileges.\n",
            GLOBAL_PATH
        );
    }
    else {
        ut_log("#[green]OK#[reset]   install bake script to '" GLOBAL_PATH "'\n");
    }

    free(script_path);

    return 0;
error:
    return -1;
}
#else
int16_t bake_create_script(void)
{
    char *script_path = ut_envparse("~/bake/bake.sh");
    if (!script_path) {
        goto error;
    }

    FILE *f = fopen (script_path, "w");
    if (!f) {
        ut_error("cannot open '%s': %s", script_path, strerror(errno));
        goto error;
    }

    fprintf(f, "#!/bin/bash\n\n");

    fprintf(f, "UNAME=$(uname)\n\n");

    fprintf(f, "clone_bake() {\n");
    fprintf(f, "    if [ ! -d \"bake\" ]; then\n");
    fprintf(f, "        echo \"Cloning bake repository...\"\n");
    fprintf(f, "        git clone -q \"https://github.com/SanderMertens/bake.git\"\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "    else\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "        echo \"Reset bake repository...\"\n");
    fprintf(f, "        git fetch -q origin\n");
    fprintf(f, "        git reset -q --hard origin/master\n");
    fprintf(f, "        git clean -q -xdf\n");
    fprintf(f, "    fi\n");
    fprintf(f, "}\n\n");

    fprintf(f, "build_bake() {\n");
    fprintf(f, "    if [ \"$UNAME\" = \"Linux\" ]; then\n");
    fprintf(f, "        make -C build-Linux\n");
    fprintf(f, "    elif [ \"$UNAME\" = \"Darwin\" ]; then\n");
    fprintf(f, "        make -C build-Darwin\n");
    fprintf(f, "    fi\n");
    fprintf(f, "}\n\n");

    fprintf(f, "install_bake() {\n");
    fprintf(f, "    ./bake setup --local-setup\n");
    fprintf(f, "}\n\n");

    fprintf(f, "if [ \"$1\" = \"upgrade\" ]; then\n");
    fprintf(f, "    mkdir -p $HOME/bake/src\n");
    fprintf(f, "    cd $HOME/bake/src\n");
    fprintf(f, "    clone_bake\n");
    fprintf(f, "    build_bake\n");
    fprintf(f, "    install_bake\n");
    fprintf(f, "else\n");
    fprintf(f, "    exec $HOME/bake/"BAKE_EXEC" \"$@\"\n");
    fprintf(f, "fi\n");
    fclose(f);

    /* Make executable for everyone */
    if (ut_setperm(script_path, 0755)) {
        ut_raise();
        ut_log("failed to set permissions of %s script\n", script_path);
    }

    /* Copy file to global location, may ask for password */
    bake_message(UT_WARNING, "WARNING", "#[normal]copying script to '" GLOBAL_PATH "', #[yellow]setup may request password");
    bake_message(UT_LOG, "", " for local-only install, just press Enter until setup continues");

    char *cp_cmd = ut_asprintf("sudo cp %s %s/bake", script_path, GLOBAL_PATH);

    if(cmd(cp_cmd)) {
        bake_message(UT_WARNING, "",
            "Failed to instal bake script to %s. Setup will continue, but\n"
            "          before you can use bake, you need to run:\n"
            "            #[normal]export PATH=$PATH:$HOME/bake\n", GLOBAL_PATH
        );
    } else {
        bake_message(UT_OK, "done", "install bake script to '" GLOBAL_PATH "'");
    }

    free(script_path);
    free(cp_cmd);

    return 0;
error:
    return -1;
}
#endif

int16_t bake_build_make_project(
    const char *path,
    const char *id,
    const char *artefact)
{
    char *install_cmd = ut_asprintf(
      "bake install %s --id %s --package --includes include",
      path, id);
    ut_try( cmd(install_cmd), "failed to install '%s' include files", id);
    free(install_cmd);
    bake_message(UT_OK, "done", "install include files for '%s'", id);
    fflush(stdout);

    char *make_cmd;

#ifdef _WIN32
    char *cwd = ut_strdup(ut_cwd());
    char *driver_path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"build-%s", ut_cwd(), path, UT_OS_STRING);
    make_cmd = ut_asprintf("nmake /NOLOGO /F Makefile clean all");
    ut_try( ut_chdir(driver_path), NULL);
#else
    make_cmd = ut_asprintf("make -C %s/build-%s clean all", path, UT_OS_STRING);
#endif

    ut_try( cmd(make_cmd), "failed to build '%s'", id);
    free(make_cmd);

#ifdef _WIN32
    ut_try( ut_chdir(cwd), NULL);
    free(cwd);
#endif

    bake_message(UT_OK, "done", "build '%s'", id);

    char *bin_path = ut_asprintf("%s"UT_OS_PS"bin"UT_OS_PS"%s-debug", path, UT_PLATFORM_STRING);
    ut_try(ut_mkdir(bin_path), "failed to create bin path for %s", id);

    /* On macOS, remove rpath that premake automatically adds */
    /*#ifdef UT_MACOS
      install_cmd = ut_asprintf(
          "install_name_tool -change @rpath/libbake_util.dylib libbake_util.dylib %s/lib%s%s",
          path, artefact, UT_OS_LIB_EXT);
      ut_try( cmd(install_cmd), NULL);
    #endif
    */

    ut_try (ut_rename(
      strarg("%s" UT_OS_PS UT_OS_LIB_PREFIX "%s" UT_OS_LIB_EXT, path, artefact),
      strarg("%s" UT_OS_PS UT_OS_LIB_PREFIX "%s" UT_OS_LIB_EXT, bin_path, artefact)),
        "failed to move '%s' to project bin path", id);

#ifdef _WIN32
    ut_try(ut_rename(
        strarg("%s" UT_OS_PS UT_OS_LIB_PREFIX "%s.lib", path, artefact),
        strarg("%s" UT_OS_PS UT_OS_LIB_PREFIX "%s.lib", bin_path, artefact)),
            "failed to move '%s' to project bin path", id);
#endif

    free(bin_path);

    install_cmd = ut_asprintf(
        "bake install %s --id %s --artefact %s --package",
        path, id, artefact);
    ut_try(cmd(install_cmd), "failed to install bake %s library", id);

    bake_message(UT_OK, "done", "install '%s' to bake environment", id);
    free(install_cmd);

    return 0;
error:
    return -1;
}

int16_t bake_setup(
    const char *bake_cmd,
    bool local)
{
    char *dir = ut_strdup(bake_cmd);
    char *last_elem = strrchr(dir, UT_OS_PS[0]);
    if (last_elem) {
        *last_elem = '\0';
        ut_chdir(dir);
    }

    ut_setenv("BAKE_SETUP", "true");

    /* Temporary fix to ensure bake shell script is updated on upgrade */
    local = false;

    char *cur_env = ut_envparse("~"UT_OS_PS"bake");
    char *cur_dir = ut_strdup(ut_cwd());

    if (!strcmp(cur_env, cur_dir)) {
        char *tmp_dir = ut_envparse("~"UT_OS_PS"bake_tmp");
        char *target_dir = ut_envparse("~"UT_OS_PS"bake"UT_OS_PS"src"UT_OS_PS"bake");
        ut_try(ut_rm(tmp_dir), NULL);
        ut_try(ut_rename(cur_dir, tmp_dir), NULL);
        ut_try(ut_mkdir("~"UT_OS_PS"bake"UT_OS_PS"src"), NULL);
        ut_try(ut_rename(tmp_dir, target_dir), NULL);
        ut_try(ut_chdir(target_dir), NULL);
        free(target_dir);
        free(tmp_dir);
    }

    free(cur_dir);

    ut_try( ut_mkdir(cur_env), NULL);
    free (cur_env);

    bake_message(UT_LOG, "", "Bake setup, installing to ~/bake");

    if (!local) {
        ut_try( bake_create_script(), "failed to create global bake script");
    }

    ut_try( ut_cp("./bake" UT_OS_BIN_EXT, "~/bake/bake2" UT_OS_BIN_EXT),
        "failed to copy bake executable");

    bake_message(UT_OK, "done", "copy bake executable");

    ut_try( cmd("bake install --id bake --includes include"),
        "failed to install bake include files");
    bake_message(UT_OK, "done", "install bake include files");

    ut_try( bake_build_make_project("util", "bake.util", "bake_util"), NULL);

    ut_try( bake_build_make_project("drivers"UT_OS_PS"lang"UT_OS_PS"c", "bake.lang.c", "bake_lang_c"), NULL);

    ut_try( bake_build_make_project("drivers"UT_OS_PS"lang"UT_OS_PS"cpp", "bake.lang.cpp", "bake_lang_cpp"), NULL);

    ut_try(cmd("./bake drivers/test"), NULL);
    bake_message(UT_OK, "done", "install test framework");

    ut_try(cmd("./bake libraries"), NULL);
    bake_message(UT_OK, "done", "install library configuration packages");

    ut_try(cmd("./bake templates"), NULL);
    bake_message(UT_OK, "done", "install template packages");

    /*

     ______   ______   ______   __   __       ______   ______  ______
    /\  __ \ /\  ___\ /\  ___\ /\ \ /\ \     /\  __ \ /\  == \/\__  _\
    \ \  __ \\ \___  \\ \ \____\ \ \\ \ \    \ \  __ \\ \  __<\/_/\ \/
     \ \_\ \_\\/\_____\\ \_____\\ \_\\ \_\    \ \_\ \_\\ \_\ \_\ \ \_\
      \/_/\/_/ \/_____/ \/_____/ \/_/ \/_/     \/_/\/_/ \/_/ /_/  \/_/

    */

    ut_log(
        "#[white]\n"
        "#[normal]    #[cyan]___      ___      ___      ___ \n"
        "#[normal]   /\\#[cyan]  \\    #[normal]/\\#[cyan]  \\    #[normal]/\\#[cyan]__\\    #[normal]/\\  #[cyan]\\ \n"
        "#[normal]  /  \\#[cyan]  \\  #[normal]/  \\#[cyan]  \\  #[normal]/ / #[cyan]_/_  #[normal]/  \\  #[cyan]\\ \n"
        "#[normal] /  \\ \\#[cyan]__\\#[normal]/  \\ \\#[cyan]__\\#[normal]/  -\"\\#[cyan]__\\#[normal]/  \\ \\#[cyan]__\\ \n"
        "#[normal] \\ \\  /#[cyan]  /#[normal]\\/\\  /#[cyan]  /#[normal]\\; ;-\"#[cyan],-\"#[normal]\\ \\ \\/  #[cyan]/ \n"
        "#[normal]  \\  /#[cyan]  /   #[normal]/ /  #[cyan]/  #[normal]| |  #[cyan]|   #[normal]\\ \\/  #[cyan]/ \n"
        "#[normal]   \\/#[cyan]__/    #[normal]\\/#[cyan]__/    #[normal]\\|#[cyan]__|    #[normal]\\/#[cyan]__/ \n\n");

    printf("\n       Installation complete \n\n");

    return 0;
error:
    return -1;
}
