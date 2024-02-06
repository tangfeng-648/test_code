# 将鼠标虚拟成触摸
## 编译
gcc mouse2touch.c -lX11 -pthread -o mouse2touch
## 介绍
将鼠标事件转发成触摸的点击事件
## TODO
1. touch事件需要update,否则无法拖动
2. 鼠标事件leftbutton和触摸事件重合，永远双击
