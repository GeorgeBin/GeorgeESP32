# 示例编译到执行步骤



## 命令行

配置环境

source "/Users/george/.espressif/tools/activate_idf_v6.0.sh"



编译：

idf.py build



烧录+看日志：
idf.py -p /dev/cu.usbserial-142101 flash monitor



## Clion

选择“app” ，点击锤子，进行 build

选择“flash”，点击锤子，进行刷固件
