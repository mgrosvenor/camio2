#! /bin/bash

if [ "$#" -lt "2" ]; then
    echo "usage: new_driver.sh <name_long> <name_short>"
    echo "   eg: new_driver.sh \"wizbanger\" \"wzb\" "
    exit -1;
fi

long_name=$1
short_name=$2
new_dir="src/drivers/$long_name"
tmp_dir="src/drivers/ttt"

#make the directory
mkdir -vp $new_dir

#copy the template files in
for f in $(ls $tmp_dir); do 
    cp -v ${tmp_dir}/$f ${new_dir}/$(echo $f | sed -e "s/ttt/${short_name}/")
done 

#replace all the ttt's 
short_name_up=$(echo $short_name | tr '[:lower:]' '[:upper:]')
perl -pi -w -e "s/ttt/$short_name/g;" $new_dir/*
perl -pi -w -e "s/TTT/$short_name_up/g;" $new_dir/*

#put the dates right
d=$(date +%Y)
perl -pi -w -e "s/ZZZZ/$d/g;" $new_dir/*

d=$(date  +"%d %b")
perl -pi -w -e "s/PPP/$d/g;" $new_dir/*



