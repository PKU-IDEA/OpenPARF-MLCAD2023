#!/usr/bin/env bash

for dir in ./Design_*;
do
   f="$dir/design.macros";
   if [ -f "$f" ];
   then
      echo "editing $f";
      grep -vE 'DSP_CASCADE.*reg|reg.*DSP_CASCADE' $f | sed '/DSP_CASCADE.*reg/d; /reg.*DSP_CASCADE/d' > /tmp/temp_file && \mv /tmp/temp_file $f;
   fi
done