set(srcs "main.c" "my_flatform.c")

set(requires "bluepad32" "btstack")

idf_component_register(SRCS "${srcs}"
        INCLUDE_DIRS "."
        REQUIRES "${requires}")
