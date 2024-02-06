# 通过matedesktop库获取屏幕信息
## 编译
gcc get_display_info.c `pkg-config --cflags mate-desktop-2.0` -lmate-desktop-2 -lgdk-3 -o get_display_info
## 介绍
试用了mate的库来读取屏幕信息,这是比较老旧且上层的库
## TODO
仅作验证,无TODO