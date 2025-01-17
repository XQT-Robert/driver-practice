#!/bin/bash

#
# @name         ：模块快速编译挂载脚本
# @Usages       ：./make_and_mount.sh
# @description  ：
#   1、编译模块，并移动到挂载目录下
#   2、如果有对应的调试 APP，自动编译，其命名格式为 (模块名称)_App ，比如 led_App,并复制进模块文件夹
#      挂载的路径是 ~/linux/nfs/rootfs/lib/modules/5.4.31/driver_test/(模块名称)/
#   3、模块名根据以测试程序（模块名称）_APP索引获取，强制要求各模块唯一拥有一个调试程序
# @configuration：
#	1、修改目录挂载目录module_path，一般修改为开发板所挂载的根文件系统的路径
# @notice  		：
#   修改相对路径应基于控制脚本ez_control的位置，并在控制脚本所处路径下访问该脚本。
#---

#必须要指定环境变量，确保脚本make可以找到，推荐使用source来指定当前脚本的环境变量和系统一致
#使用下面的方法可能会受当前环境变量影响，导致和系统的环境变量不一致的情况下添加环境变量，出现未知错误
#export PATH=$PATH:/usr/local/arm/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin
source /etc/profile
echo "Current PATH: $PATH"

make clean > /dev/null
make > /dev/null

#根据APP名称更改模块名,仅匹配第一个
#project_name=$(find . -name "*_App*" | sed 's|^\./||' | sed 's|\.c$||')
project_name=$(find . -name "*_App*" | head -n 1 | \
                                       xargs -n 1 basename -s .c |\
                                       sed 's/_App$//'
                                       )

# 设置创建模块路径,这里的路径一定要使用绝对路径,否则sudo或者一些意外情况会导致访问错误
module_path=/home/robert/linux/nfs/rootfs/lib/modules/5.4.31/driver_test/$project_name
echo "Building...！module_path : $module_path"
sudo mkdir -p "$module_path"

# 检查是否有调试 APP 的源文件，并进行编译
if [ -e "${project_name}_App.c" ]; then 
    echo "Found ${project_name}_App.c, compiling..."
    arm-none-linux-gnueabihf-gcc "${project_name}_App.c" -o "${project_name}_App"
    # 如果需要将编译后的 APP 也复制到挂载路径
    sudo cp "${project_name}_App" "$module_path"
else
    echo "No ${project_name}_App.c found."
fi


# 获取参数 (ko_file) -- 找到当前目录下的 .ko 文件名，去掉路径前缀及后缀
# 脚本中不存在数组，任意一个变量都可以作为“数组”使用，各变量之间以空格隔开
# 比如 array = (1 2 3 4 5)，就是一个数组,其中array[1] = 2;
ko_files=$(find . -name "*.ko" | sed 's|^\./||' | sed 's|\.ko$||')

#复制所有模块到模块地址
for ko_file in $ko_files; do
    echo "moving moudle $ko_file now .."
    sudo cp "$ko_file.ko" "${module_path}"
done


