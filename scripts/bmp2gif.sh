#!/usr/bin/env bash


# generate a array, the content is 00, 01, 03, 04, 05
plot_dir="./plot/mlcad2023/Design_170"

# for each element in array, do the following
array=(00 01 03 04 05)
for i in ${array[@]}
do
    ffmpeg -y -i ${plot_dir}/iter%03d0_at${i}.bmp -vf palettegen=reserve_transparent=off ${plot_dir}/palette.png
    ffmpeg -y -framerate 20 -i ${plot_dir}/iter%03d0_at${i}.bmp -i ${plot_dir}/palette.png -filter_complex paletteuse -loop 0 ${plot_dir}/at${i}.gif
done

array=(00 01)
for i in ${array[@]}
do
    ffmpeg -y -i ${plot_dir}/fgrain_iter%03d0_gid-${i}.bmp -vf palettegen=reserve_transparent=off ${plot_dir}/palette.png
    ffmpeg -y -framerate 20 -i ${plot_dir}/fgrain_iter%03d0_gid-${i}.bmp -i ${plot_dir}/palette.png -filter_complex paletteuse -loop 0 ${plot_dir}/a_gid-${i}.gif
done