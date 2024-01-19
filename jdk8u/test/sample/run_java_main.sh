#!/bin/bash
set -e

CUR_DIR=`cd $(dirname $0) && pwd`
. ${CUR_DIR}/set_java_env.sh

script=$0
print_usage() {
  filename=`basename $script`
  echo "Usage: sh ./$filename <Java_Main_Class> [Output_Log_File]"
  exit 1
}

arg_count=$#
echo "arg_count=$arg_count"
if [ "$arg_count" -eq 0 ]; then
  print_usage
fi
Java_Main_Class=$1
# 将.替换为/
main_class_path=${Java_Main_Class//./\/}
full_main_class_fila_path=${java_target_dir}/${main_class_path}.class
if [ ! -f "$full_main_class_fila_path" ]; then
  echo "JavaMainClass[${Java_Main_Class}] not exist..."
  exit 1
fi

Output_Log_File=""
if [ "$arg_count" -eq 2 ]; then
  Output_Log_File=$2
fi


# 指定运行参数...
program_arguments="-Dslog.level=NONE -Xmx50m -Xms50m -XX:-UseAdaptiveSizePolicy -XX:+PrintGCDetails"
echo "program_arguments:\n\t${program_arguments}"
java_program_arguments="-cp ${java_target_dir} ${program_arguments} ${Java_Main_Class}"
echo "java_program_arguments:\n\t${java_program_arguments}"
if [ -n "$Output_Log_File" ]; then
  echo "result output to ${Output_Log_File}..."
  $openjdk_home/bin/java ${java_program_arguments} > ${Output_Log_File}
else
  $openjdk_home/bin/java ${java_program_arguments}
fi
