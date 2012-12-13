#!/bin/bash
src_dir=${PWD}/../../../../src/p2p/peer
version_dir=${src_dir}/../../ppbox/ppbox
file_prefix=autopeerversion
build_version=`LANG=C svn info ${version_dir} | awk -F : '$1 == "Revision" { print $2}'`
build_version=`echo ${build_version}`
old_header=${src_dir}/${file_prefix}.h
new_header=${src_dir}/${file_prefix}.hbak
cp ${src_dir}/${file_prefix}.hpp ${new_header}

if [ `uname` = 'Darwin' ]; then
    sed -i '' "s/\\\$WCREV\\\$/${build_version}/g" ${new_header}
else
    sed -i "s/\\\$WCREV\\\$/${build_version}/g" ${new_header}
fi


if [ ! -e "$old_header" ];then
	touch $old_header
fi

count=`diff $old_header $new_header | wc -l`
if [ $count -gt 0 ]; then
	mv $new_header $old_header
else
	rm -f $new_header
fi



