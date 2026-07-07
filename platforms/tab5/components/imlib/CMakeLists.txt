idf_component_register(
    SRCS 
        "src/draw.c"
        "src/font.c" 
        "src/fmath.c" 
        "src/imlib.c" 
    INCLUDE_DIRS "include"     # 头文件目录
    EMBED_FILES 
        unicode_font16x16.bin
)

