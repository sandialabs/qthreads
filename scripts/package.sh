#!/bin/bash
shopt -s extglob

tag=$1

while [ ! -f ./autogen.sh ]; do 
    cd ..
    qthread_root=`pwd`
done

if [ ! -f ./autogen.sh ]; then
    echo "Error: I can't figure out where I am. Please run this script from the root of the qthreads source directory"
    exit 1
fi

base_dir_name=${PWD##*/}
base_dir_name=${base_dir_name:-/}

linked_files="./compile \
              ./config.guess \
              ./config.sub \
              ./depcomp \
              ./install-sh \
              ./libtool.m4 \
              ./ltmain.sh \
              ./lt~obsolete.m4 \
              ./ltoptions.m4 \
              ./ltsugar.m4 \
              ./ltversion.m4 \
              ./missing \
              ./test-driver"

echo "./autogen"
bash ./autogen.sh
mkdir $base_dir_name-$tag
echo "mkdir $base_dir_name-$tag"
cp -r !($base_dir_name-$tag) $base_dir_name-$tag/
cd $base_dir_name-$tag
    echo "rm -r ./autom4te.cache"
    rm -rf ./autom4te.cache 
    echo "Replacing links to autotool scripts with actual scripts"
    cd config
        mkdir tmp
        for  i in $linked_files
        do
            echo $i
            cp -Lr $i tmp/$i
            mv tmp/$i $i
        done
    rm -r tmp
    cd .. #config dir
    echo "Removing hidden files"
    rm -rf .[a-z]*
    echo "Update .autogen-version"
    rm .autogen-version
    echo "$tag" > .autogen-version
cd .. #$base_dir_name-$tag
echo "tar cjf $base_dir_name-$tag.tar.gz -C .. $base_dir_name-$tag"
tar cjf $base_dir_name-$tag.tar.gz -C . $base_dir_name-$tag 

