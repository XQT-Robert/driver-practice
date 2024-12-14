#!/bin/bash

#
# @name         ：辅助控制脚本
# @Usages       ：./ez_control.sh [控制命令] [命令所需参数]
# @description  ：
#   作为接口用于辅助使用具体的功能脚本，调用目录下的scripts文件夹的一个脚本功能
# @configuration：
#	使用前需要修改确认你所需的脚本，正常位于路径./scripts
# @notice  		：
#   使用本脚本时，需要./script文件夹下查看对应脚本的设置是否正确，
#	调用脚本中默认设置路径是基于此脚本路径设置的，修改脚本时请注意脚本路径设置问题，
#	脚本中的相对路径是相对于执行脚本时你所处的命令行的路径而言的，
#	因此不建议在该文件目录外调用该脚本进行控制，请在脚本目录下直接运行此脚本，
#	如需修改，对应路径应该按照此控制脚本路径修改。
#---

if [ ! -d ./scripts ]; then
	echo "脚本文件丢失！"
	exit 1;
fi

echo "Make sure you have configured the script!"
echo "you should go to the ./script and modify the corresponding script"


if [ -z "$1" ]; then
    echo "缺少控制命令，exit"
    exit 1
fi

#功能菜单
case "$1" in
copy)
	echo "命令：复制当前模块"
	echo "----下面是调用脚本信息："
	if [ -z "$2" ]; then
    	echo "该命令需要参数$2,请给出新模块名"
    	exit 1
	fi
	./scripts/copy_the_file.sh "$2";;
make)
	echo "命令：编译并复制模块文件到挂载目录"
	echo "----下面是调用脚本信息："
	./scripts/make_and_mount.sh;;
gdb)
	echo "命令：调试模块"
	echo "----下面是调用脚本信息："
esac
