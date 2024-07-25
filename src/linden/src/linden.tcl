# exposing envflags

namespace eval linden::env {

proc tmp_file_root_dir {} {
    linden::env_tmp_file_root_dir
}

}