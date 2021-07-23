#!/bin/bash
PROJ_PATH=`git rev-parse --show-toplevel`

if [ 0"$LD_LIBRARY_PATH" = "0" ]; then
    export LD_LIBRARY_PATH=$PROJ_PATH/src/Banyan/banyan-integrate
else 
    export LD_LIBRARY_PATH=$PROJ_PATH/src/Banyan/banyan-integrate:$LD_LIBRARY_PATH
fi

../../../build/src/eden -exit banyan.tcl