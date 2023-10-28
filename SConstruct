import os
import scripts.app_helper as app

helper = app.Helper(ARGUMENTS);
helper.set_dll_def('src/ftp_fs.def').set_libs(['ftp_fs']).call(DefaultEnvironment)

SConscriptFiles = ['src/SConscript', 'demos/SConscript']
helper.SConscript(SConscriptFiles)
