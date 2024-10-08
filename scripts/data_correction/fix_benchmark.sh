#!/usr/bin/env bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <source_dir>"
    exit 1
fi

# check if python is in your PATYH, if not, exit
if ! command -v python &> /dev/null
then
    echo "python could not be found"
    exit
fi

source_dir=$1
correct_script_dir="${source_dir}/scripts/data_correction"

csh ${correct_script_dir}/clean_regions.csh

\cp ${correct_script_dir}/design.lib .
\cp ${correct_script_dir}/design.scl .
for dir in ./Design_*; do \rm "$dir/design.lib"; \cp ./design.lib "$dir"; done
for dir in ./Design_*; do \rm "$dir/design.scl"; \cp ./design.scl "$dir"; done

bash ${correct_script_dir}/clean_reg.sh

for dir in ./Design_*;
do
   f="$dir/design.cascade_shape_instances";
   if [ -f "$f" ];
   then
      echo "editing $f";
      \cp $f "$f.bk";
      sed -i "s/BRAM_CASCADE 2 1/BRAM_CASCADE_2 2 1/g" $f;
   fi
done

python ${correct_script_dir}/one_pin_nets_in_design_nets.py

python ${correct_script_dir}/remedy_order.py

python ${correct_script_dir}/cascade_macro_replenish.py
