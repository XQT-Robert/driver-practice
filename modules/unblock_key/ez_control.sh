#!/bin/bash

#
# @name         ：辅助控制脚本
# @Usages       ：./quick_mod_test.sh [控制选项]
# @description  ：
#   作为接口用于辅助使用具体的功能脚本，调用目录下的script文件夹的一个脚本功能
# @configuration：
#	使用前需要修改确认你所需的脚本，正常位于路径./script
# @notice  		：
#   使用本脚本时，需要./script文件夹下查看对应脚本的设置是否正确，
#	调用脚本中默认设置路径是基于此脚本路径设置的，修改脚本时请注意脚本路径设置问题，
#	脚本中的相对路径是相对于执行脚本时你所处的命令行的路径而言的，
#	因此不建议在该文件目录外调用该脚本进行控制，请在脚本目录下直接运行此脚本，
#	如需修改，对应路径应该按照此控制脚本路径修改。
#---

ehco "Make sure you have configured the script!"
ehco "you should go to the ./script and modify the corresponding script"

if [ -z "$1" ]; then
    echo "缺少控制命令，exit"
    exit 1
fi