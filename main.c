#include <stdint.h>
#include "stm32f10x.h"

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---

// Начальное значение ARR: 4999 -> (4999+1) / 10000 Гц = 0.5 сек.
// Переключение PC13 каждые 0.5 сек -> период мигания 1 сек.
volatile uint16_t timer_arr_value = 4999;

// Лимиты для изменения частоты
const uint16_t ARR_MAX_VALUE = 4999; // 1 секунда период
const uint16_t ARR_MIN_LIMIT = 49;   // 10 мс период (0.01 сек)

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

#ifndef __NOP
#define __NOP() (void)0
#endif

// Короткая задержка для устранения дребезга кнопки (debouncing)
void delay_us(uint32_t us) {
    // При 72 МГц, ~72 цикла на 1 микросекунду
    for (volatile uint32_t i = 0; i < us * 72; i++) {
        __NOP();
    }
}

// --- ИНИЦИАЛИЗАЦИЯ ТАЙМЕРА TIM2 ---

void tim2_init(uint16_t arr_value) {
    // 1. Включаем тактирование TIM2 и AFIO
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    // 2. Настраиваем делитель (Prescaler)
    // 72 МГц / (7199 + 1) = 10 000 Гц (частота счетчика)
    TIM2->PSC = 7199;

    // 3. Устанавливаем ARR
    TIM2->ARR = arr_value;

    // 4. Разрешаем прерывание по событию обновления (Update Interrupt)
    TIM2->DIER |= TIM_DIER_UIE;

    // 5. Разрешаем прерывание TIM2 в NVIC
    NVIC_EnableIRQ(TIM2_IRQn);

    // 6. Генерируем событие обновления, чтобы загрузить PSC и ARR
    TIM2->EGR |= TIM_EGR_UG;

    // 7. Запускаем таймер
    TIM2->CR1 |= TIM_CR1_CEN;
}

// --- ОБРАБОТЧИК ПРЕРЫВАНИЯ ТАЙМЕРА (ISR) ---

// Эта функция должна быть определена, чтобы таймер работал
void TIM2_IRQHandler(void) {
    // Проверяем и очищаем флаг прерывания обновления
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;

        // Переключаем состояние пина PC13 (светодиод)
        GPIOC->ODR ^= (1U << 13U);
    }
}

// --- ОСНОВНАЯ ПРОГРАММА ---

int main(void) {
    // 1. Включаем тактирование портов C, B и AFIO
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    // 2. Настраиваем PC13 (светодиод) как выход (Output, 10Mhz)
    GPIOC->CRH &= ~GPIO_CRH_CNF13; // clear CNF bits
    GPIOC->CRH |= GPIO_CRH_MODE13_0; // Max speed = 10Mhz (PC13 находится в CRH)

    // 3. Настраиваем PB0 (кнопка) как вход (Floating Input)
    GPIOB->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0); // Очищаем биты MODE и CNF
    GPIOB->CRL |= GPIO_CRL_CNF0_0; // Устанавливаем CNF в Floating Input (PB0 находится в CRL)

    // 4. Инициализируем таймер TIM2 с максимальным периодом
    tim2_init(ARR_MAX_VALUE);

    while (1) {
        // Проверяем нажата ли кнопка (PB0).
        // Если PB0=0, считаем, что кнопка нажата.
        if ((GPIOB->IDR & GPIO_IDR_IDR0) == 0) {

            // Если текущий период больше минимального, уменьшаем его в ~2 раза
            if (timer_arr_value > ARR_MIN_LIMIT) {
                // Деление на 2: (+1 для округления)
                timer_arr_value = (timer_arr_value + 1) / 2 - 1;
                if (timer_arr_value < ARR_MIN_LIMIT) timer_arr_value = ARR_MIN_LIMIT;
            } else {
                 // Если достигли минимума, сбрасываем до максимума
                 timer_arr_value = ARR_MAX_VALUE;
            }

            // Обновляем регистр ARR таймера, чтобы изменить частоту
            TIM2->ARR = timer_arr_value;

            // Debouncing: ждем короткое время и ждем отпускания кнопки
            delay_us(500); // 500 мкс задержка
            while ((GPIOB->IDR & GPIO_IDR_IDR0) == 0) {
                __NOP();
            }
            delay_us(500); // Дополнительный debouncing
        }

        // Основной цикл теперь пуст, так как мигание управляется прерыванием
        __NOP();
    }
}
