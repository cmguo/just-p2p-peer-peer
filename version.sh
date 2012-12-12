#!/bin/bash

src_dir=${PWD}/../../../../src/p2p/peer
version_dir=${src_dir}/../../ppbox/ppbox
file_prefix=autopeerversion
build_version=`LANG=C svn info ${version_dir} | awk -F : '$1 == "Revision" { print $2}'`
build_version=`echo ${build_version}`
cp ${src_dir}/${file_prefix}.hpp ${src_dir}/${file_prefix}.h
sed -i "s/\\\$WCREV\\\$/${build_version}/g" ${src_dir}/${file_prefix}.h
