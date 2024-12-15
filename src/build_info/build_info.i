%{
#include "build_info.hpp"
%}

%inline %{

const char* get_build_time() {
    return EDEN_BUILD_TIME;
}

const char* get_commit_id() {
    return EDEN_COMMIT_ID;
}

%}
