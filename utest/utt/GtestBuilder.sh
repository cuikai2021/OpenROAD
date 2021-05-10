#!/bin/bash

initTestFile(){
    echo "#include <itest.h>" > ${file}
    echo "" >> ${file}
    echo "using namespace std;" >> ${file}
    echo "" >> ${file}
    echo "TEST(${dir}_,test0){" >> ${file}
    echo "  EXPECT_EQ(0,0);" >> ${file}
    echo "}" >> ${file}
    echo "" >> ${file}
}

buildHeaderCMake(){
    for index in $(grep "add_executable(unittest" -n  CMakeLists.txt | cut -d ":" -f 1)
    do
        sed "${index} a ${file}" -i CMakeLists.txt
    done
}

buildTestFile(){
    if ! [ -f ${file} ]; then
        touch ${file}
        echo "build file ${file}"
        initTestFile
        buildHeaderCMake
    else
        echo "${file} is already existed"
    fi
}

if ! [ -d src ]; then
    echo "did not find <src> folder"
    exit 1
fi

target_path="../utest"
mkdir -p ${target_path}

cd src

for dir in $(ls .)
do  
    if [ ${dir} == "cmake" ]; then
        echo "won't build unittest for <cmake>"
    elif [ -d ${dir} ]; then
        mkdir -p ${target_path}/${dir}
        file=${target_path}/${dir}/EdenTest${dir^^}.cc
        buildTestFile
    fi
done



