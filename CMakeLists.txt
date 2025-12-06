cmake_minimum_required(VERSION 3.16)

# =============================================================
# 1. НАСТРОЙКА КРОСС-КОМПИЛЯЦИИ
# =============================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)

project(stm32_timer_blink C)

# Настройка стандартов
set(CMAKE_C_STANDARD 11)

# Флаги компиляции для STM32F103 (Cortex-M3)
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")
# Определяем чип как Medium Density (для stm32f10x.h)
set(MCU_DEFS "-DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER")

# Флаги компилятора
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} ${MCU_DEFS} -Os -ffunction-sections -fdata-sections -Wall -Wextra")
# Флаги линкера
set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -Wl,--gc-sections -nostdlib -specs=nosys.specs")

# =============================================================
# 2. ИСТОЧНИКИ И ИСПОЛНЯЕМЫЙ ФАЙЛ
# =============================================================

set(SOURCES
        main.c
        stm32/startup/startup_stm32f10x_md.c
        stm32/system_stm32f10x.c
        syscalls/syscalls.c
)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
        ${CMAKE_SOURCE_DIR}/stm32
        ${CMAKE_SOURCE_DIR}/CMSIS
        ${CMAKE_SOURCE_DIR}/syscalls
)

# =============================================================
# 3. ЛИНКЕРНЫЙ СКРИПТ (ИСПРАВЛЕНИЕ: ДОБАВЛЕНИЕ _sidata)
# =============================================================

# Линкерный скрипт для STM32F103C8 (64K flash, 20K RAM)
file(WRITE ${CMAKE_BINARY_DIR}/linker.ld [[
/* Определение памяти */
MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Секции */
SECTIONS {
    /* Векторная таблица и код */
    .text : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;           /* Конец секции текста */
        _sidata = _etext;     /* !!! ИСПРАВЛЕНО: Определяем Flash-адрес .data для startup файла !!! */
    } > FLASH

    /* Инициализированные данные */
    .data : AT (_sidata) {    /* Используем _sidata как Load Address */
        . = ALIGN(4);
        _sdata = .;           /* Начало данных в RAM (Run Address) */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;           /* Конец данных в RAM */
    } > RAM

    /* Неинициализированные данные (BSS) */
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Указатель стека */
    _stack_end = ORIGIN(RAM) + LENGTH(RAM);
}
]])

# Опции линковки
target_link_options(${PROJECT_NAME}.elf PRIVATE
        -T${CMAKE_BINARY_DIR}/linker.ld
        -Wl,-Map=${PROJECT_NAME}.map
)

# ИСПРАВЛЕНИЕ ОШИБКИ 'memcpy': Явно линкуем библиотеки 'c' и 'm'
target_link_libraries(${PROJECT_NAME}.elf PRIVATE
        c
        m
)


# =============================================================
# 4. ПОСТ-СБОРОЧНЫЕ ЦЕЛИ (ЯВНЫЕ)
# =============================================================

# Создание целевого файла .BIN (добавлено ALL для сборки по умолчанию)
add_custom_target(bin ALL
        COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Создание ${PROJECT_NAME}.bin"
)

# Создание целевого файла .HEX (добавлено ALL для сборки по умолчанию)
add_custom_target(hex ALL
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
        DEPENDS ${PROJECT_NAME}.elf
        COMMENT "Создание ${PROJECT_NAME}.hex"
)

# Цель для просмотра размера
add_custom_target(size
        DEPENDS ${PROJECT_NAME}.elf
        COMMAND arm-none-eabi-size ${PROJECT_NAME}.elf
        COMMENT "Использование памяти:"
)

# Цель для прошивки через ST-Link (для реального железа)
add_custom_target(flash
        DEPENDS ${PROJECT_NAME}.bin
        COMMAND openocd
        -f interface/stlink-v2.cfg
        -f target/stm32f1x.cfg
        -c "program ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin verify reset exit 0x08000000"
        COMMENT "Прошивка устройства..."
)
